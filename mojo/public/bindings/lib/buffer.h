// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_BUFFER_H_
#define MOJO_PUBLIC_BINDINGS_LIB_BUFFER_H_

#include <stddef.h>

#include <deque>

#include "mojo/public/system/macros.h"

namespace mojo {

// Buffer provides a way to allocate memory. Allocations are 8-byte aligned and
// zero-initialized. Allocations remain valid for the lifetime of the Buffer.
class Buffer {
 public:
  typedef void (*Destructor)(void* address);

  Buffer();
  virtual ~Buffer();

  virtual void* Allocate(size_t num_bytes, Destructor func = NULL) = 0;

  static Buffer* current();

 private:
  Buffer* previous_;
};

namespace internal {

// The following class is designed to be allocated on the stack.  If necessary,
// it will failover to allocating objects on the heap.
class ScratchBuffer : public Buffer {
 public:
  ScratchBuffer();
  virtual ~ScratchBuffer();

  virtual void* Allocate(size_t num_bytes, Destructor func = NULL)
      MOJO_OVERRIDE;

 private:
  enum { kMinSegmentSize = 512 };

  struct Segment {
    Segment* next;
    char* cursor;
    char* end;
  };

  void* AllocateInSegment(Segment* segment, size_t num_bytes);
  void AddOverflowSegment(size_t delta);

  char fixed_data_[kMinSegmentSize];
  Segment fixed_;
  Segment* overflow_;

  struct PendingDestructor {
    Destructor func;
    void* address;
  };
  std::deque<PendingDestructor> pending_dtors_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScratchBuffer);
};

// FixedBuffer provides a simple way to allocate objects within a fixed chunk
// of memory. Objects are allocated by calling the |Allocate| method, which
// extends the buffer accordingly. Objects allocated in this way are not freed
// explicitly. Instead, they remain valid so long as the FixedBuffer remains
// valid.  The Leak method may be used to steal the underlying memory from the
// FixedBuffer.
//
// Typical usage:
//
//   {
//     FixedBuffer buf(8 + 8);
//
//     int* a = static_cast<int*>(buf->Allocate(sizeof(int)));
//     *a = 2;
//
//     double* b = static_cast<double*>(buf->Allocate(sizeof(double)));
//     *b = 3.14f;
//
//     void* data = buf.Leak();
//     Process(data);
//
//     free(data);
//   }
//
class FixedBuffer : public Buffer {
 public:
  explicit FixedBuffer(size_t size);
  virtual ~FixedBuffer();

  // Grows the buffer by |num_bytes| and returns a pointer to the start of the
  // addition. The resulting address is 8-byte aligned, and the content of the
  // memory is zero-filled.
  virtual void* Allocate(size_t num_bytes, Destructor func = NULL)
      MOJO_OVERRIDE;

  size_t size() const { return size_; }

  // Returns the internal memory owned by the Buffer to the caller. The Buffer
  // relinquishes its pointer, effectively resetting the state of the Buffer
  // and leaving the caller responsible for freeing the returned memory address
  // when no longer needed.
  void* Leak();

 private:
  char* ptr_;
  size_t cursor_;
  size_t size_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(FixedBuffer);
};

}  // namespace internal

class AllocationScope {
 public:
  AllocationScope() {}
  ~AllocationScope() {}

  Buffer* buffer() { return &buffer_; }

 private:
  internal::ScratchBuffer buffer_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(AllocationScope);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_BUFFER_H_
