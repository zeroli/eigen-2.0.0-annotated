// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Copyright (C) 2008-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2009 Kenneth Riddile <kfriddile@yahoo.com>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_MEMORY_H
#define EIGEN_MEMORY_H

#if defined(__APPLE__) || defined(_WIN64)
  #define EIGEN_MALLOC_ALREADY_ALIGNED 1
#else
  #define EIGEN_MALLOC_ALREADY_ALIGNED 0
#endif

#if (defined _GNU_SOURCE) || ((defined _XOPEN_SOURCE) && (_XOPEN_SOURCE >= 600))
  #define EIGEN_HAS_POSIX_MEMALIGN 1
#else
  #define EIGEN_HAS_POSIX_MEMALIGN 0
#endif

#ifdef EIGEN_VECTORIZE_SSE
  #define EIGEN_HAS_MM_MALLOC 1
#else
  #define EIGEN_HAS_MM_MALLOC 0
#endif

/** \internal like malloc, but the returned pointer is guaranteed to be 16-byte aligned.
  * Fast, but wastes 16 additional bytes of memory.
  * Does not throw any exception.
  */
inline void* ei_handmade_aligned_malloc(size_t size)
{
  void *original = malloc(size+16);  // 返回的指针最小会align到8个字节边界
  // 先align down 到16个字节边界，然后再align up到16个字节边界
  // 如果original已经是align到16个字节边界，那么aligned将会是第2个16字节
  // 接下来会在第二个16字节边界处往前8个字节处存储原始个8字节指针
  void *aligned = reinterpret_cast<void*>((reinterpret_cast<size_t>(original) & ~(size_t(15))) + 16);
  *(reinterpret_cast<void**>(aligned) - 1) = original;
  return aligned;
}

/** \internal frees memory allocated with ei_handmade_aligned_malloc */
inline void ei_handmade_aligned_free(void *ptr)
{
  if(ptr)
    free(*(reinterpret_cast<void**>(ptr) - 1));
}

/** \internal allocates \a size bytes. The returned pointer is guaranteed to have 16 bytes alignment.
  * On allocation error, the returned pointer is undefined, but if exceptions are enabled then a std::bad_alloc is thrown.
  */
inline void* ei_aligned_malloc(size_t size)
{
  #ifdef EIGEN_NO_MALLOC
    ei_assert(false && "heap allocation is forbidden (EIGEN_NO_MALLOC is defined)");
  #endif

  void *result;
  #if EIGEN_HAS_POSIX_MEMALIGN && !EIGEN_MALLOC_ALREADY_ALIGNED
    #ifdef EIGEN_EXCEPTIONS
      const int failed =
    #endif
    posix_memalign(&result, 16, size);
  #else
    #if EIGEN_MALLOC_ALREADY_ALIGNED
      result = malloc(size);
    #elif EIGEN_HAS_MM_MALLOC
      result = _mm_malloc(size, 16);
    #elif (defined _MSC_VER)
      result = _aligned_malloc(size, 16);
    #else
      result = ei_handmade_aligned_malloc(size);
    #endif
    #ifdef EIGEN_EXCEPTIONS
      const int failed = (result == 0);
    #endif
  #endif
  #ifdef EIGEN_EXCEPTIONS
    if(failed)
      throw std::bad_alloc();
  #endif
  return result;
}

/** allocates \a size bytes. If Align is true, then the returned ptr is 16-byte-aligned.
  * On allocation error, the returned pointer is undefined, but if exceptions are enabled then a std::bad_alloc is thrown.
  */
template<bool Align> inline void* ei_conditional_aligned_malloc(size_t size)
{
  return ei_aligned_malloc(size);
}

template<> inline void* ei_conditional_aligned_malloc<false>(size_t size)
{
  #ifdef EIGEN_NO_MALLOC
    ei_assert(false && "heap allocation is forbidden (EIGEN_NO_MALLOC is defined)");
  #endif

  void *result = malloc(size);
  #ifdef EIGEN_EXCEPTIONS
    if(!result) throw std::bad_alloc();
  #endif
  return result;
}

/** allocates \a size objects of type T. The returned pointer is guaranteed to have 16 bytes alignment.
  * On allocation error, the returned pointer is undefined, but if exceptions are enabled then a std::bad_alloc is thrown.
  * The default constructor of T is called.
  */
template<typename T> inline T* ei_aligned_new(size_t size)
{
  void *void_result = ei_aligned_malloc(sizeof(T)*size);
  return ::new(void_result) T[size];
}

template<typename T, bool Align> inline T* ei_conditional_aligned_new(size_t size)
{
  void *void_result = ei_conditional_aligned_malloc<Align>(sizeof(T)*size);
  return ::new(void_result) T[size];
}

/** \internal free memory allocated with ei_aligned_malloc
  */
inline void ei_aligned_free(void *ptr)
{
  #if EIGEN_MALLOC_ALREADY_ALIGNED
    free(ptr);
  #elif EIGEN_HAS_POSIX_MEMALIGN
    free(ptr);
  #elif EIGEN_HAS_MM_MALLOC
    _mm_free(ptr);
  #elif defined(_MSC_VER)
    _aligned_free(ptr);
  #else
    ei_handmade_aligned_free(ptr);
  #endif
}

/** \internal free memory allocated with ei_conditional_aligned_malloc
  */
template<bool Align> inline void ei_conditional_aligned_free(void *ptr)
{
  ei_aligned_free(ptr);
}

template<> inline void ei_conditional_aligned_free<false>(void *ptr)
{
  free(ptr);
}

/** \internal delete the elements of an array.
  * The \a size parameters tells on how many objects to call the destructor of T.
  */
template<typename T> inline void ei_delete_elements_of_array(T *ptr, size_t size)
{
  // always destruct an array starting from the end.
  while(size) ptr[--size].~T();
}

/** \internal delete objects constructed with ei_aligned_new
  * The \a size parameters tells on how many objects to call the destructor of T.
  */
template<typename T> inline void ei_aligned_delete(T *ptr, size_t size)
{
  ei_delete_elements_of_array<T>(ptr, size);
  ei_aligned_free(ptr);
}

/** \internal delete objects constructed with ei_conditional_aligned_new
  * The \a size parameters tells on how many objects to call the destructor of T.
  */
template<typename T, bool Align> inline void ei_conditional_aligned_delete(T *ptr, size_t size)
{
  ei_delete_elements_of_array<T>(ptr, size);
  ei_conditional_aligned_free<Align>(ptr);
}

/** \internal \returns the number of elements which have to be skipped such that data are 16 bytes aligned */
template<typename Scalar>
inline static int ei_alignmentOffset(const Scalar* ptr, int maxOffset)
{
  typedef typename ei_packet_traits<Scalar>::type Packet;
  const int PacketSize = ei_packet_traits<Scalar>::size;
  const int PacketAlignedMask = PacketSize-1;
  const bool Vectorized = PacketSize>1;
  return Vectorized
          ? std::min<int>( (PacketSize - (int((size_t(ptr)/sizeof(Scalar))) & PacketAlignedMask))
                           & PacketAlignedMask, maxOffset)
          : 0;
}

/** \internal
  * ei_aligned_stack_alloc(SIZE) allocates an aligned buffer of SIZE bytes
  * on the stack if SIZE is smaller than EIGEN_STACK_ALLOCATION_LIMIT.
  * Otherwise the memory is allocated on the heap.
  * Data allocated with ei_aligned_stack_alloc \b must be freed by calling ei_aligned_stack_free(PTR,SIZE).
  * \code
  * float * data = ei_aligned_stack_alloc(float,array.size());
  * // ...
  * ei_aligned_stack_free(data,float,array.size());
  * \endcode
  */
#ifdef __linux__
  #define ei_aligned_stack_alloc(SIZE) (SIZE<=EIGEN_STACK_ALLOCATION_LIMIT) \
                                    ? alloca(SIZE) \
                                    : ei_aligned_malloc(SIZE)
  #define ei_aligned_stack_free(PTR,SIZE) if(SIZE>EIGEN_STACK_ALLOCATION_LIMIT) ei_aligned_free(PTR)
#else
  #define ei_aligned_stack_alloc(SIZE) ei_aligned_malloc(SIZE)
  #define ei_aligned_stack_free(PTR,SIZE) ei_aligned_free(PTR)
#endif

#define ei_aligned_stack_new(TYPE,SIZE) ::new(ei_aligned_stack_alloc(sizeof(TYPE)*SIZE)) TYPE[SIZE]
#define ei_aligned_stack_delete(TYPE,PTR,SIZE) do {ei_delete_elements_of_array<TYPE>(PTR, SIZE); \
                                                   ei_aligned_stack_free(PTR,sizeof(TYPE)*SIZE);} while(0)


/** \brief Overloads the operator new and delete of the class Type with operators that are aligned if NeedsToAlign is true
  *
  * When Eigen's explicit vectorization is enabled, Eigen assumes that some fixed sizes types are aligned
  * on a 16 bytes boundary. Those include all Matrix types having a sizeof multiple of 16 bytes, e.g.:
  *  - Vector2d, Vector4f, Vector4i, Vector4d,
  *  - Matrix2d, Matrix4f, Matrix4i, Matrix4d,
  *  - etc.
  * When an object is statically allocated, the compiler will automatically and always enforces 16 bytes
  * alignment of the data when needed. However some troubles might appear when data are dynamically allocated.
  * Let's pick an example:
  * \code
  * struct Foo {
  *   char dummy;
  *   Vector4f some_vector;
  * };
  * Foo obj1;                           // static allocation
  * obj1.some_vector = Vector4f(..);    // =>   OK
  *
  * Foo *pObj2 = new Foo;               // dynamic allocation
  * pObj2->some_vector = Vector4f(..);  // =>  !! might segfault !!
  * \endcode
  * Here, the problem is that operator new is not aware of the compile time alignment requirement of the
  * type Vector4f (and hence of the type Foo). Therefore "new Foo" does not necessarily returns a 16 bytes
  * aligned pointer. The purpose of the class WithAlignedOperatorNew is exactly to overcome this issue by
  * overloading the operator new to return aligned data when the vectorization is enabled.
  * Here is a similar safe example:
  * \code
  * struct Foo {
  *   EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  *   char dummy;
  *   Vector4f some_vector;
  * };
  * Foo *pObj2 = new Foo;               // dynamic allocation
  * pObj2->some_vector = Vector4f(..);  // =>  SAFE !
  * \endcode
  *
  * \sa class ei_new_allocator
  */
#define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(NeedsToAlign) \
    void *operator new(size_t size) throw() { \
      return Eigen::ei_conditional_aligned_malloc<NeedsToAlign>(size); \
    } \
    void *operator new[](size_t size) throw() { \
      return Eigen::ei_conditional_aligned_malloc<NeedsToAlign>(size); \
    } \
    void operator delete(void * ptr) { Eigen::ei_conditional_aligned_free<NeedsToAlign>(ptr); } \
    void operator delete[](void * ptr) { Eigen::ei_conditional_aligned_free<NeedsToAlign>(ptr); } \
    void *operator new(size_t, void *ptr) throw() { return ptr; }

#define EIGEN_MAKE_ALIGNED_OPERATOR_NEW EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(true)
#define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(Scalar,Size) \
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(((Size)!=Eigen::Dynamic) && ((sizeof(Scalar)*(Size))%16==0))

/** \class aligned_allocator
*
* \brief stl compatible allocator to use with with 16 byte aligned types
*
* Example:
* \code
* // Matrix4f requires 16 bytes alignment:
* std::map< int, Matrix4f, std::less<int>, aligned_allocator<Matrix4f> > my_map_mat4;
* // Vector3f does not require 16 bytes alignment, no need to use Eigen's allocator:
* std::map< int, Vector3f > my_map_vec3;
* \endcode
*
*/
template<class T>
class aligned_allocator
{
public:
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
    typedef T*        pointer;
    typedef const T*  const_pointer;
    typedef T&        reference;
    typedef const T&  const_reference;
    typedef T         value_type;

    template<class U>
    struct rebind
    {
        typedef aligned_allocator<U> other;
    };

    pointer address( reference value ) const
    {
        return &value;
    }

    const_pointer address( const_reference value ) const
    {
        return &value;
    }

    aligned_allocator() throw()
    {
    }

    aligned_allocator( const aligned_allocator& ) throw()
    {
    }

    template<class U>
    aligned_allocator( const aligned_allocator<U>& ) throw()
    {
    }

    ~aligned_allocator() throw()
    {
    }

    size_type max_size() const throw()
    {
        return std::numeric_limits<size_type>::max();
    }

    pointer allocate( size_type num, const_pointer* hint = 0 )
    {
        static_cast<void>( hint ); // suppress unused variable warning
        return static_cast<pointer>( ei_aligned_malloc( num * sizeof(T) ) );
    }

    void construct( pointer p, const T& value )
    {
        ::new( p ) T( value );
    }

    void destroy( pointer p )
    {
        p->~T();
    }

    void deallocate( pointer p, size_type /*num*/ )
    {
        ei_aligned_free( p );
    }
};

#endif // EIGEN_MEMORY_H
