// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/discardable_shared_memory_heap.h"

#include <algorithm>

#include "base/memory/discardable_shared_memory.h"

namespace content {
namespace {

bool IsPowerOfTwo(size_t x) {
  return (x & (x - 1)) == 0;
}

bool IsInFreeList(DiscardableSharedMemoryHeap::Span* span) {
  return span->previous() || span->next();
}

}  // namespace

DiscardableSharedMemoryHeap::Span::Span(
    base::DiscardableSharedMemory* shared_memory,
    size_t start,
    size_t length)
    : shared_memory_(shared_memory), start_(start), length_(length) {
}

DiscardableSharedMemoryHeap::Span::~Span() {
}

DiscardableSharedMemoryHeap::ScopedMemorySegment::ScopedMemorySegment(
    DiscardableSharedMemoryHeap* heap,
    scoped_ptr<base::DiscardableSharedMemory> shared_memory,
    size_t size,
    const base::Closure& deleted_callback)
    : heap_(heap),
      shared_memory_(shared_memory.Pass()),
      size_(size),
      deleted_callback_(deleted_callback) {
}

DiscardableSharedMemoryHeap::ScopedMemorySegment::~ScopedMemorySegment() {
  heap_->ReleaseMemory(shared_memory_.get(), size_);
  deleted_callback_.Run();
}

bool DiscardableSharedMemoryHeap::ScopedMemorySegment::IsUsed() const {
  return heap_->IsMemoryUsed(shared_memory_.get(), size_);
}

bool DiscardableSharedMemoryHeap::ScopedMemorySegment::IsResident() const {
  return heap_->IsMemoryResident(shared_memory_.get());
}

DiscardableSharedMemoryHeap::DiscardableSharedMemoryHeap(size_t block_size)
    : block_size_(block_size), num_blocks_(0), num_free_blocks_(0) {
  DCHECK_NE(block_size_, 0u);
  DCHECK(IsPowerOfTwo(block_size_));
}

DiscardableSharedMemoryHeap::~DiscardableSharedMemoryHeap() {
  memory_segments_.clear();
  DCHECK_EQ(num_blocks_, 0u);
  DCHECK_EQ(num_free_blocks_, 0u);
  DCHECK_EQ(std::count_if(free_spans_, free_spans_ + arraysize(free_spans_),
                          [](const base::LinkedList<Span>& free_spans) {
                            return !free_spans.empty();
                          }),
            0);
}

scoped_ptr<DiscardableSharedMemoryHeap::Span> DiscardableSharedMemoryHeap::Grow(
    scoped_ptr<base::DiscardableSharedMemory> shared_memory,
    size_t size,
    const base::Closure& deleted_callback) {
  // Memory must be aligned to block size.
  DCHECK_EQ(
      reinterpret_cast<size_t>(shared_memory->memory()) & (block_size_ - 1),
      0u);
  DCHECK_EQ(size & (block_size_ - 1), 0u);

  scoped_ptr<Span> span(
      new Span(shared_memory.get(),
               reinterpret_cast<size_t>(shared_memory->memory()) / block_size_,
               size / block_size_));
  DCHECK(spans_.find(span->start_) == spans_.end());
  DCHECK(spans_.find(span->start_ + span->length_ - 1) == spans_.end());
  RegisterSpan(span.get());

  num_blocks_ += span->length_;

  // Start tracking if segment is resident by adding it to |memory_segments_|.
  memory_segments_.push_back(new ScopedMemorySegment(this, shared_memory.Pass(),
                                                     size, deleted_callback));

  return span.Pass();
}

void DiscardableSharedMemoryHeap::MergeIntoFreeLists(scoped_ptr<Span> span) {
  DCHECK(span->shared_memory_);

  // First add length of |span| to |num_free_blocks_|.
  num_free_blocks_ += span->length_;

  // Merge with previous span if possible.
  SpanMap::iterator prev_it = spans_.find(span->start_ - 1);
  if (prev_it != spans_.end() && IsInFreeList(prev_it->second)) {
    scoped_ptr<Span> prev = RemoveFromFreeList(prev_it->second);
    DCHECK_EQ(prev->start_ + prev->length_, span->start_);
    UnregisterSpan(prev.get());
    if (span->length_ > 1)
      spans_.erase(span->start_);
    span->start_ -= prev->length_;
    span->length_ += prev->length_;
    spans_[span->start_] = span.get();
  }

  // Merge with next span if possible.
  SpanMap::iterator next_it = spans_.find(span->start_ + span->length_);
  if (next_it != spans_.end() && IsInFreeList(next_it->second)) {
    scoped_ptr<Span> next = RemoveFromFreeList(next_it->second);
    DCHECK_EQ(next->start_, span->start_ + span->length_);
    UnregisterSpan(next.get());
    if (span->length_ > 1)
      spans_.erase(span->start_ + span->length_ - 1);
    span->length_ += next->length_;
    spans_[span->start_ + span->length_ - 1] = span.get();
  }

  InsertIntoFreeList(span.Pass());
}

scoped_ptr<DiscardableSharedMemoryHeap::Span>
DiscardableSharedMemoryHeap::Split(Span* span, size_t blocks) {
  DCHECK(blocks);
  DCHECK_LT(blocks, span->length_);

  scoped_ptr<Span> leftover(new Span(
      span->shared_memory_, span->start_ + blocks, span->length_ - blocks));
  DCHECK_IMPLIES(leftover->length_ > 1,
                 spans_.find(leftover->start_) == spans_.end());
  RegisterSpan(leftover.get());
  spans_[span->start_ + blocks - 1] = span;
  span->length_ = blocks;
  return leftover.Pass();
}

scoped_ptr<DiscardableSharedMemoryHeap::Span>
DiscardableSharedMemoryHeap::SearchFreeLists(size_t blocks, size_t slack) {
  DCHECK(blocks);

  size_t length = blocks;
  size_t max_length = blocks + slack;

  // Search array of free lists for a suitable span.
  while (length - 1 < arraysize(free_spans_) - 1) {
    const base::LinkedList<Span>& free_spans = free_spans_[length - 1];
    if (!free_spans.empty()) {
      // Return the most recently used span located in tail.
      return Carve(free_spans.tail()->value(), blocks);
    }

    // Return early after surpassing |max_length|.
    if (++length > max_length)
      return nullptr;
  }

  const base::LinkedList<Span>& overflow_free_spans =
      free_spans_[arraysize(free_spans_) - 1];

  // Search overflow free list for a suitable span. Starting with the most
  // recently used span located in tail and moving towards head.
  for (base::LinkNode<Span>* node = overflow_free_spans.tail();
       node != overflow_free_spans.end(); node = node->previous()) {
    Span* span = node->value();
    if (span->length_ >= blocks && span->length_ <= max_length)
      return Carve(span, blocks);
  }

  return nullptr;
}

void DiscardableSharedMemoryHeap::ReleaseFreeMemory() {
  // Erase all free segments after rearranging the segments in such a way
  // that used segments precede all free segments.
  memory_segments_.erase(
      std::partition(
          memory_segments_.begin(), memory_segments_.end(),
          [](const ScopedMemorySegment* segment) { return segment->IsUsed(); }),
      memory_segments_.end());
}

void DiscardableSharedMemoryHeap::ReleasePurgedMemory() {
  // Erase all purged segments after rearranging the segments in such a way
  // that resident segments precede all purged segments.
  memory_segments_.erase(
      std::partition(memory_segments_.begin(), memory_segments_.end(),
                     [](const ScopedMemorySegment* segment) {
                       return segment->IsResident();
                     }),
      memory_segments_.end());
}

size_t DiscardableSharedMemoryHeap::GetSize() const {
  return num_blocks_ * block_size_;
}

size_t DiscardableSharedMemoryHeap::GetSizeOfFreeLists() const {
  return num_free_blocks_ * block_size_;
}

void DiscardableSharedMemoryHeap::InsertIntoFreeList(
    scoped_ptr<DiscardableSharedMemoryHeap::Span> span) {
  DCHECK(!IsInFreeList(span.get()));
  size_t index = std::min(span->length_, arraysize(free_spans_)) - 1;
  free_spans_[index].Append(span.release());
}

scoped_ptr<DiscardableSharedMemoryHeap::Span>
DiscardableSharedMemoryHeap::RemoveFromFreeList(Span* span) {
  DCHECK(IsInFreeList(span));
  span->RemoveFromList();
  return make_scoped_ptr(span);
}

scoped_ptr<DiscardableSharedMemoryHeap::Span>
DiscardableSharedMemoryHeap::Carve(Span* span, size_t blocks) {
  scoped_ptr<Span> serving = RemoveFromFreeList(span);

  const int extra = serving->length_ - blocks;
  if (extra) {
    scoped_ptr<Span> leftover(
        new Span(serving->shared_memory_, serving->start_ + blocks, extra));
    DCHECK_IMPLIES(extra > 1, spans_.find(leftover->start_) == spans_.end());
    RegisterSpan(leftover.get());

    // No need to coalesce as the previous span of |leftover| was just split
    // and the next span of |leftover| was not previously coalesced with
    // |span|.
    InsertIntoFreeList(leftover.Pass());

    serving->length_ = blocks;
    spans_[serving->start_ + blocks - 1] = serving.get();
  }

  // |serving| is no longer in the free list, remove its length from
  // |num_free_blocks_|.
  DCHECK_GE(num_free_blocks_, serving->length_);
  num_free_blocks_ -= serving->length_;

  return serving.Pass();
}

void DiscardableSharedMemoryHeap::RegisterSpan(Span* span) {
  spans_[span->start_] = span;
  if (span->length_ > 1)
    spans_[span->start_ + span->length_ - 1] = span;
}

void DiscardableSharedMemoryHeap::UnregisterSpan(Span* span) {
  DCHECK(spans_.find(span->start_) != spans_.end());
  DCHECK_EQ(spans_[span->start_], span);
  spans_.erase(span->start_);
  if (span->length_ > 1) {
    DCHECK(spans_.find(span->start_ + span->length_ - 1) != spans_.end());
    DCHECK_EQ(spans_[span->start_ + span->length_ - 1], span);
    spans_.erase(span->start_ + span->length_ - 1);
  }
}

bool DiscardableSharedMemoryHeap::IsMemoryUsed(
    const base::DiscardableSharedMemory* shared_memory,
    size_t size) {
  size_t offset =
      reinterpret_cast<size_t>(shared_memory->memory()) / block_size_;
  size_t length = size / block_size_;
  DCHECK(spans_.find(offset) != spans_.end());
  Span* span = spans_[offset];
  DCHECK_LE(span->length_, length);
  // Memory is used if first span is not in free list or shorter than segment.
  return !IsInFreeList(span) || span->length_ != length;
}

bool DiscardableSharedMemoryHeap::IsMemoryResident(
    const base::DiscardableSharedMemory* shared_memory) {
  return shared_memory->IsMemoryResident();
}

void DiscardableSharedMemoryHeap::ReleaseMemory(
    const base::DiscardableSharedMemory* shared_memory,
    size_t size) {
  size_t offset =
      reinterpret_cast<size_t>(shared_memory->memory()) / block_size_;
  size_t end = offset + size / block_size_;
  while (offset < end) {
    DCHECK(spans_.find(offset) != spans_.end());
    Span* span = spans_[offset];
    DCHECK_EQ(span->shared_memory_, shared_memory);
    span->shared_memory_ = nullptr;
    UnregisterSpan(span);

    offset += span->length_;

    DCHECK_GE(num_blocks_, span->length_);
    num_blocks_ -= span->length_;

    // If |span| is in the free list, remove it and update |num_free_blocks_|.
    if (IsInFreeList(span)) {
      DCHECK_GE(num_free_blocks_, span->length_);
      num_free_blocks_ -= span->length_;
      RemoveFromFreeList(span);
    }
  }
}

}  // namespace content
