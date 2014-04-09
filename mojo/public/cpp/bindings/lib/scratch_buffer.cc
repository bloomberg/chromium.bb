// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/scratch_buffer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "mojo/public/cpp/bindings/lib/bindings_serialization.h"

// Scrub memory in debug builds to help catch use-after-free bugs.
#ifdef NDEBUG
#define DEBUG_SCRUB(address, size) (void) (address), (void) (size)
#else
#define DEBUG_SCRUB(address, size) memset(address, 0xCD, size)
#endif

namespace mojo {
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
  if (!result && overflow_)
    result = AllocateInSegment(overflow_, delta);

  if (!result && AddOverflowSegment(delta))
    result = AllocateInSegment(overflow_, delta);

  if (func && result) {
    PendingDestructor dtor;
    dtor.func = func;
    dtor.address = result;
    pending_dtors_.push_back(dtor);
  }
  return result;
}

void* ScratchBuffer::AllocateInSegment(Segment* segment, size_t delta) {
  if (static_cast<size_t>(segment->end - segment->cursor) >= delta) {
    void* result = segment->cursor;
    memset(result, 0, delta);  // Required to avoid info leaks.
    segment->cursor += delta;
    return result;
  }
  return NULL;
}

bool ScratchBuffer::AddOverflowSegment(size_t delta) {
  if (delta < kMinSegmentSize)
    delta = kMinSegmentSize;

  if (delta > kMaxSegmentSize)
    return false;

  // Ensure segment buffer is aligned.
  size_t segment_size = internal::Align(sizeof(Segment)) + delta;
  Segment* segment = static_cast<Segment*>(malloc(segment_size));
  if (segment) {
    segment->next = overflow_;
    segment->cursor = reinterpret_cast<char*>(segment + 1);
    segment->end = segment->cursor + delta;
    overflow_ = segment;
    return true;
  }

  return false;
}

}  // namespace internal
}  // namespace mojo
