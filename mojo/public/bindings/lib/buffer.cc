// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/buffer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "mojo/public/bindings/lib/bindings_serialization.h"
#include "mojo/public/bindings/lib/bindings_support.h"

// Scrub memory in debug builds to help catch use-after-free bugs.
#ifdef NDEBUG
#define DEBUG_SCRUB(address, size) (void) (address), (void) (size)
#else
#define DEBUG_SCRUB(address, size) memset(address, 0xCD, size)
#endif

namespace mojo {

//-----------------------------------------------------------------------------

Buffer::Buffer() {
  previous_ = BindingsSupport::Get()->SetCurrentBuffer(this);
}

Buffer::~Buffer() {
  Buffer* buf MOJO_ALLOW_UNUSED =
      BindingsSupport::Get()->SetCurrentBuffer(previous_);
  assert(buf == this);
}

Buffer* Buffer::current() {
  return BindingsSupport::Get()->GetCurrentBuffer();
}

//-----------------------------------------------------------------------------

namespace internal {

ScratchBuffer::ScratchBuffer()
    : overflow_(NULL) {
  fixed_.next = NULL;
  fixed_.cursor = fixed_data_;
  fixed_.end = fixed_data_ + kMinSegmentSize;
}

ScratchBuffer::~ScratchBuffer() {
  // Invoke destructors in reverse order to mirror allocation order.
  std::deque<PendingDestructor>::reverse_iterator it;
  for (it = pending_dtors_.rbegin(); it != pending_dtors_.rend(); ++it)
    it->func(it->address);

  while (overflow_) {
    Segment* doomed = overflow_;
    overflow_ = overflow_->next;
    DEBUG_SCRUB(doomed, doomed->end - reinterpret_cast<char*>(doomed));
    free(doomed);
  }
  DEBUG_SCRUB(fixed_data_, sizeof(fixed_data_));
}

void* ScratchBuffer::Allocate(size_t delta, Destructor func) {
  delta = internal::Align(delta);

  void* result = AllocateInSegment(&fixed_, delta);
  if (!result) {
    if (overflow_)
      result = AllocateInSegment(overflow_, delta);

    if (!result) {
      AddOverflowSegment(delta);
      result = AllocateInSegment(overflow_, delta);
    }
  }

  if (func) {
    PendingDestructor dtor;
    dtor.func = func;
    dtor.address = result;
    pending_dtors_.push_back(dtor);
  }
  return result;
}

void* ScratchBuffer::AllocateInSegment(Segment* segment, size_t delta) {
  void* result;
  if (static_cast<size_t>(segment->end - segment->cursor) >= delta) {
    result = segment->cursor;
    memset(result, 0, delta);
    segment->cursor += delta;
  } else {
    result = NULL;
  }
  return result;
}

void ScratchBuffer::AddOverflowSegment(size_t delta) {
  if (delta < kMinSegmentSize)
    delta = kMinSegmentSize;

  // Ensure segment buffer is aligned.
  size_t segment_size = internal::Align(sizeof(Segment)) + delta;

  Segment* segment = static_cast<Segment*>(malloc(segment_size));
  segment->next = overflow_;
  segment->cursor = reinterpret_cast<char*>(segment + 1);
  segment->end = segment->cursor + delta;

  overflow_ = segment;
}

//-----------------------------------------------------------------------------

FixedBuffer::FixedBuffer(size_t size)
    : ptr_(NULL),
      cursor_(0),
      size_(internal::Align(size)) {
  ptr_ = static_cast<char*>(calloc(size_, 1));
}

FixedBuffer::~FixedBuffer() {
  free(ptr_);
}

void* FixedBuffer::Allocate(size_t delta, Destructor dtor) {
  assert(!dtor);

  delta = internal::Align(delta);

  // TODO(darin): Using <assert.h> is probably not going to cut it.
  assert(delta > 0);
  assert(cursor_ + delta <= size_);
  if (cursor_ + delta > size_)
    return NULL;

  char* result = ptr_ + cursor_;
  cursor_ += delta;

  return result;
}

void* FixedBuffer::Leak() {
  char* ptr = ptr_;
  ptr_ = NULL;
  cursor_ = 0;
  size_ = 0;
  return ptr;
}

}  // namespace internal
}  // namespace mojo
