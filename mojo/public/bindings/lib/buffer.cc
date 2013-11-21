// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/buffer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "mojo/public/bindings/lib/bindings_serialization.h"

namespace mojo {

//-----------------------------------------------------------------------------

ScratchBuffer::ScratchBuffer()
    : overflow_(NULL) {
  fixed_.next = NULL;
  fixed_.cursor = fixed_data_;
  fixed_.end = fixed_data_ + kMinSegmentSize;
}

ScratchBuffer::~ScratchBuffer() {
  while (overflow_) {
    Segment* doomed = overflow_;
    overflow_ = overflow_->next;
    free(doomed);
  }
}

void* ScratchBuffer::Allocate(size_t delta) {
  delta = internal::Align(delta);

  void* result = AllocateInSegment(&fixed_, delta);
  if (result)
    return result;

  if (overflow_) {
    result = AllocateInSegment(overflow_, delta);
    if (result)
      return result;
  }

  AddOverflowSegment(delta);
  return AllocateInSegment(overflow_, delta);
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

void* FixedBuffer::Allocate(size_t delta) {
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

}  // namespace mojo
