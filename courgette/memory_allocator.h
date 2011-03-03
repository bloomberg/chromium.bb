// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_MEMORY_ALLOCATOR_H_
#define COURGETTE_MEMORY_ALLOCATOR_H_

#include <memory>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/platform_file.h"

namespace courgette {

#ifdef OS_WIN

// Manages a temporary file.  The file is created in the %TEMP% folder and
// is deleted when the file handle is closed.
// NOTE: Since the file will be used as backing for a memory allocation,
// it will never be so big that size_t cannot represent its size.
class TempFile {
 public:
  TempFile();
  ~TempFile();

  __declspec(noinline) void Create();
  void Close();
  __declspec(noinline) void SetSize(size_t size);

  // Returns true iff the temp file is currently open.
  bool valid() const;

  // Returns the handle of the temporary file or INVALID_HANDLE_VALUE if
  // a temp file has not been created.
  base::PlatformFile handle() const;

  // Returns the size of the temp file.  If the temp file doesn't exist,
  // the return value is 0.
  size_t size() const;

 protected:
  __declspec(noinline) FilePath PrepareTempFile();

  base::PlatformFile file_;
  size_t size_;
};

// Manages a read/write virtual mapping of a physical file.
class FileMapping {
 public:
  FileMapping();
  ~FileMapping();

  // Map a file from beginning to |size|.
  __declspec(noinline) void Create(HANDLE file, size_t size);
  void Close();

  // Returns true iff a mapping has been created.
  bool valid() const;

  // Returns a writable pointer to the beginning of the memory mapped file.
  // If Create has not been called successfully, return value is NULL.
  void* view() const;

 protected:
  __declspec(noinline) void InitializeView(size_t size);

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
  // process' address space.
  __declspec(noinline) void Initialize(size_t size);

  // Returns a writable pointer to the reserved memory.
  void* memory() const;

  // Returns a pointer to the TempMapping instance that allocated the |mem|
  // block of memory.  It's the callers responsibility to make sure that
  // the memory block was allocated by the TempMapping class.
  static TempMapping* GetMappingFromPtr(void* mem);

 protected:
  TempFile file_;
  FileMapping mapping_;
};

// An STL compatible memory allocator class that allocates memory either
// from the heap or via a temporary file.  A file allocation will be made
// if either the requested memory size exceeds |kMaxHeapAllocationSize|
// or if a heap allocation fails.
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
    // convert an MemoryAllocator<T> to a MemoryAllocator<OtherT>
    typedef MemoryAllocator<OtherT> other;
  };

  MemoryAllocator() _THROW0() {
  }

  explicit MemoryAllocator(const MemoryAllocator<T>& other) _THROW0() {
  }

  template<class OtherT>
  explicit MemoryAllocator(const MemoryAllocator<OtherT>& other) _THROW0() {
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
      throw std::length_error("overflow");

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
      TempMapping* mapping = new TempMapping();
      mapping->Initialize(bytes);
      mem = reinterpret_cast<uint8*>(mapping->memory());
      mem[0] = static_cast<uint8>(FILE_ALLOCATION);
    }
    return reinterpret_cast<pointer>(mem + sizeof(T));
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

  size_t max_size() const _THROW0() {
    size_type count = static_cast<size_type>(-1) / sizeof(T);
    return (0 < count ? count : 1);
  }
};

#else  // OS_WIN

// On Mac, Linux, we just use the default STL allocator.
template<class T>
class MemoryAllocator : public std::allocator<T> {
 public:
};

#endif  // OS_WIN

}  // namespace courgette

#endif  // COURGETTE_MEMORY_ALLOCATOR_H_
