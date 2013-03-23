// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_MEMORY_ALLOCATOR_H_
#define COURGETTE_MEMORY_ALLOCATOR_H_

#include <memory>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/platform_file.h"

#ifndef NDEBUG

// A helper class to track down call sites that are not handling error cases.
template<class T>
class CheckReturnValue {
 public:
  // Not marked explicit on purpose.
  CheckReturnValue(T value) : value_(value), checked_(false) {  // NOLINT
  }
  CheckReturnValue(const CheckReturnValue& other)
      : value_(other.value_), checked_(other.checked_) {
    other.checked_ = true;
  }

  CheckReturnValue& operator=(const CheckReturnValue& other) {
    if (this != &other) {
      DCHECK(checked_);
      value_ = other.value_;
      checked_ = other.checked_;
      other.checked_ = true;
    }
  }

  ~CheckReturnValue() {
    DCHECK(checked_);
  }

  operator const T&() const {
    checked_ = true;
    return value_;
  }

 private:
  T value_;
  mutable bool checked_;
};
typedef CheckReturnValue<bool> CheckBool;
#else
typedef bool CheckBool;
#endif

namespace courgette {

#if defined(OS_WIN)

// Manages a temporary file.  The file is created in the %TEMP% folder and
// is deleted when the file handle is closed.
// NOTE: Since the file will be used as backing for a memory allocation,
// it will never be so big that size_t cannot represent its size.
class TempFile {
 public:
  TempFile();
  ~TempFile();

  bool Create();
  void Close();
  bool SetSize(size_t size);

  // Returns true iff the temp file is currently open.
  bool valid() const;

  // Returns the handle of the temporary file or INVALID_HANDLE_VALUE if
  // a temp file has not been created.
  base::PlatformFile handle() const;

 protected:
  base::PlatformFile file_;
};

// Manages a read/write virtual mapping of a physical file.
class FileMapping {
 public:
  FileMapping();
  ~FileMapping();

  // Map a file from beginning to |size|.
  bool Create(HANDLE file, size_t size);
  void Close();

  // Returns true iff a mapping has been created.
  bool valid() const;

  // Returns a writable pointer to the beginning of the memory mapped file.
  // If Create has not been called successfully, return value is NULL.
  void* view() const;

 protected:
  bool InitializeView(size_t size);

  HANDLE mapping_;
  void* view_;
};

// Manages a temporary file and a memory mapping of the temporary file.
// The memory that this class manages holds a pointer back to the TempMapping
// object itself, so that given a memory pointer allocated by this class,
// you can get a pointer to the TempMapping instance that owns that memory.
class TempMapping {
 public:
  TempMapping();
  ~TempMapping();

  // Creates a temporary file of size |size| and maps it into the current
  // process's address space.
  bool Initialize(size_t size);

  // Returns a writable pointer to the reserved memory.
  void* memory() const;

  // Returns true if the mapping is valid and memory is available.
  bool valid() const;

  // Returns a pointer to the TempMapping instance that allocated the |mem|
  // block of memory.  It's the callers responsibility to make sure that
  // the memory block was allocated by the TempMapping class.
  static TempMapping* GetMappingFromPtr(void* mem);

 protected:
  TempFile file_;
  FileMapping mapping_;
};

// A memory allocator class that allocates memory either from the heap or via a
// temporary file.  The interface is STL inspired but the class does not throw
// STL exceptions on allocation failure.  Instead it returns NULL.
// A file allocation will be made if either the requested memory size exceeds
// |kMaxHeapAllocationSize| or if a heap allocation fails.
// Allocating the memory as a mapping of a temporary file solves the problem
// that there might not be enough physical memory and pagefile to support the
// allocation.  This can happen because these resources are too small, or
// already committed to other processes.  Provided there is enough disk, the
// temporary file acts like a pagefile that other processes can't access.
template<class T>
class MemoryAllocator {
 public:
  typedef T value_type;
  typedef value_type* pointer;
  typedef value_type& reference;
  typedef const value_type* const_pointer;
  typedef const value_type& const_reference;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  // Each allocation is tagged with a single byte so that we know how to
  // deallocate it.
  enum AllocationType {
    HEAP_ALLOCATION,
    FILE_ALLOCATION,
  };

  // 5MB is the maximum heap allocation size that we'll attempt.
  // When applying a patch for Chrome 10.X we found that at this
  // threshold there were 17 allocations higher than this threshold
  // (largest at 136MB) 10 allocations just below the threshold and 6362
  // smaller allocations.
  static const size_t kMaxHeapAllocationSize = 1024 * 1024 * 5;

  template<class OtherT>
  struct rebind {
    // convert a MemoryAllocator<T> to a MemoryAllocator<OtherT>
    typedef MemoryAllocator<OtherT> other;
  };

  MemoryAllocator() _THROW0() {
  }

  // We can't use an explicit constructor here, as dictated by our style guide.
  // The implementation of basic_string in Visual Studio 2010 prevents this.
  MemoryAllocator(const MemoryAllocator<T>& other) _THROW0() {  // NOLINT
  }

  template<class OtherT>
  MemoryAllocator(const MemoryAllocator<OtherT>& other) _THROW0() {  // NOLINT
  }

  ~MemoryAllocator() {
  }

  void deallocate(pointer ptr, size_type size) {
    uint8* mem = reinterpret_cast<uint8*>(ptr);
    mem -= sizeof(T);
    if (mem[0] == HEAP_ALLOCATION) {
      delete [] mem;
    } else {
      DCHECK_EQ(static_cast<uint8>(FILE_ALLOCATION), mem[0]);
      TempMapping* mapping = TempMapping::GetMappingFromPtr(mem);
      delete mapping;
    }
  }

  pointer allocate(size_type count) {
    // We use the first byte of each allocation to mark the allocation type.
    // However, so that the allocation is properly aligned, we allocate an
    // extra element and then use the first byte of the first element
    // to mark the allocation type.
    count++;

    if (count > max_size())
      return NULL;

    size_type bytes = count * sizeof(T);
    uint8* mem = NULL;

    // First see if we can do this allocation on the heap.
    if (count < kMaxHeapAllocationSize)
      mem = new(std::nothrow) uint8[bytes];
    if (mem != NULL) {
      mem[0] = static_cast<uint8>(HEAP_ALLOCATION);
    } else {
      // If either the heap allocation failed or the request exceeds the
      // max heap allocation threshold, we back the allocation with a temp file.
      TempMapping* mapping = new(std::nothrow) TempMapping();
      if (mapping && mapping->Initialize(bytes)) {
        mem = reinterpret_cast<uint8*>(mapping->memory());
        mem[0] = static_cast<uint8>(FILE_ALLOCATION);
      }
    }
    return mem ? reinterpret_cast<pointer>(mem + sizeof(T)) : NULL;
  }

  pointer allocate(size_type count, const void* hint) {
    return allocate(count);
  }

  void construct(pointer ptr, const T& value) {
    ::new(ptr) T(value);
  }

  void destroy(pointer ptr) {
    ptr->~T();
  }

  size_type max_size() const _THROW0() {
    size_type count = static_cast<size_type>(-1) / sizeof(T);
    return (0 < count ? count : 1);
  }
};

#else  // OS_WIN

// On Mac, Linux, we use a bare bones implementation that only does
// heap allocations.
template<class T>
class MemoryAllocator {
 public:
  typedef T value_type;
  typedef value_type* pointer;
  typedef value_type& reference;
  typedef const value_type* const_pointer;
  typedef const value_type& const_reference;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  template<class OtherT>
  struct rebind {
    // convert a MemoryAllocator<T> to a MemoryAllocator<OtherT>
    typedef MemoryAllocator<OtherT> other;
  };

  MemoryAllocator() {
  }

  explicit MemoryAllocator(const MemoryAllocator<T>& other) {
  }

  template<class OtherT>
  explicit MemoryAllocator(const MemoryAllocator<OtherT>& other) {
  }

  ~MemoryAllocator() {
  }

  void deallocate(pointer ptr, size_type size) {
    delete [] ptr;
  }

  pointer allocate(size_type count) {
    if (count > max_size())
      return NULL;
    return reinterpret_cast<pointer>(
        new(std::nothrow) uint8[count * sizeof(T)]);
  }

  pointer allocate(size_type count, const void* hint) {
    return allocate(count);
  }

  void construct(pointer ptr, const T& value) {
    ::new(ptr) T(value);
  }

  void destroy(pointer ptr) {
    ptr->~T();
  }

  size_type max_size() const {
    size_type count = static_cast<size_type>(-1) / sizeof(T);
    return (0 < count ? count : 1);
  }
};

#endif  // OS_WIN

// Manages a growable buffer.  The buffer allocation is done by the
// MemoryAllocator class.  This class will not throw exceptions so call sites
// must be prepared to handle memory allocation failures.
// The interface is STL inspired to avoid having to make too many changes
// to code that previously was using STL.
template<typename T, class Allocator = MemoryAllocator<T> >
class NoThrowBuffer {
 public:
  typedef T value_type;
  static const size_t kAllocationFailure = 0xffffffff;
  static const size_t kStartSize = sizeof(T) > 0x100 ? 1 : 0x100 / sizeof(T);

  NoThrowBuffer() : buffer_(NULL), size_(0), alloc_size_(0) {
  }

  ~NoThrowBuffer() {
    clear();
  }

  void clear() {
    if (buffer_) {
      alloc_.deallocate(buffer_, alloc_size_);
      buffer_ = NULL;
      size_ = 0;
      alloc_size_ = 0;
    }
  }

  bool empty() const {
    return size_ == 0;
  }

  CheckBool reserve(size_t size) WARN_UNUSED_RESULT {
    if (failed())
      return false;

    if (size <= alloc_size_)
      return true;

    if (size < kStartSize)
      size = kStartSize;

    T* new_buffer = alloc_.allocate(size);
    if (!new_buffer) {
      clear();
      alloc_size_ = kAllocationFailure;
    } else {
      if (buffer_) {
        memcpy(new_buffer, buffer_, size_ * sizeof(T));
        alloc_.deallocate(buffer_, alloc_size_);
      }
      buffer_ = new_buffer;
      alloc_size_ = size;
    }

    return !failed();
  }

  CheckBool append(const T* data, size_t size) WARN_UNUSED_RESULT {
    if (failed())
      return false;

    if (size > alloc_.max_size() - size_)
      return false;

    if (!size)
      return true;

    if ((alloc_size_ - size_) < size) {
      const size_t max_size = alloc_.max_size();
      size_t new_size = alloc_size_ ? alloc_size_ : kStartSize;
      while (new_size < size_ + size) {
        if (new_size < max_size - new_size) {
          new_size *= 2;
        } else {
          new_size = max_size;
        }
      }
      if (!reserve(new_size))
        return false;
    }

    memcpy(buffer_ + size_, data, size * sizeof(T));
    size_ += size;

    return true;
  }

  CheckBool resize(size_t size, const T& init_value) WARN_UNUSED_RESULT {
    if (size > size_) {
      if (!reserve(size))
        return false;
      for (size_t i = size_; i < size; ++i)
        buffer_[i] = init_value;
    } else if (size < size_) {
      // TODO(tommi): Should we allocate a new, smaller buffer?
      // It might be faster for us to simply change the size.
    }

    size_ = size;

    return true;
  }

  CheckBool push_back(const T& item) WARN_UNUSED_RESULT {
    return append(&item, 1);
  }

  const T& back() const {
    return buffer_[size_ - 1];
  }

  T& back() {
    return buffer_[size_ - 1];
  }

  const T* begin() const {
    if (!size_)
      return NULL;
    return &buffer_[0];
  }

  T* begin() {
    if (!size_)
      return NULL;
    return &buffer_[0];
  }

  const T* end() const {
    if (!size_)
      return NULL;
    return &buffer_[size_ - 1];
  }

  T* end() {
    if (!size_)
      return NULL;
    return &buffer_[size_ - 1];
  }

  const T& operator[](size_t index) const {
    DCHECK(index < size_);
    return buffer_[index];
  }

  T& operator[](size_t index) {
    DCHECK(index < size_);
    return buffer_[index];
  }

  size_t size() const {
    return size_;
  }

  T* data() const {
    return buffer_;
  }

  // Returns true if an allocation failure has ever occurred for this object.
  bool failed() const {
    return alloc_size_ == kAllocationFailure;
  }

 protected:
  T* buffer_;
  size_t size_;  // how much of the buffer we're using.
  size_t alloc_size_;  // how much space we have allocated.
  Allocator alloc_;
};

}  // namespace courgette

#endif  // COURGETTE_MEMORY_ALLOCATOR_H_
