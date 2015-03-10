// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/discardable_shared_memory_heap.h"

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
    size_t size)
    : heap_(heap), shared_memory_(shared_memory.Pass()), size_(size) {
}

DiscardableSharedMemoryHeap::ScopedMemorySegment::~ScopedMemorySegment() {
  heap_->ReleaseMemory(shared_memory_.get(), size_);
  // Purge memory. This has no effect if already purged.
  shared_memory_->Purge(base::Time::Now());
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
  DCHECK(free_spans_.empty());
}

scoped_ptr<DiscardableSharedMemoryHeap::Span> DiscardableSharedMemoryHeap::Grow(
    scoped_ptr<base::DiscardableSharedMemory> shared_memory,
    size_t size) {
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
  memory_segments_.push_back(
      new ScopedMemorySegment(this, shared_memory.Pass(), size));

  return span.Pass();
}

void DiscardableSharedMemoryHeap::MergeIntoFreeList(scoped_ptr<Span> span) {
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

  free_spans_.Append(span.release());
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
DiscardableSharedMemoryHeap::SearchFreeList(size_t blocks, size_t slack) {
  DCHECK(blocks);

  // Search through list to find best span.
  Span* best = nullptr;

  // This implements address-ordered best-fit.
  for (Span* span = free_spans_.head()->value(); span != free_spans_.end();
       span = span->next()->value()) {
    // Skip span if it's not large enough.
    if (span->length_ < blocks)
      continue;
    // Skip span if it's too large.
    if (span->length_ - blocks > slack)
      continue;

    if (best) {
      // Skip span if |best| is a better fit.
      if (span->length_ > best->length_)
        continue;

      // Skip span if |best| starts at a lower address.
      if ((span->length_ == best->length_) && (span->start_ > best->start_))
        continue;
    }

    best = span;
  }

  return best ? Carve(best, blocks) : nullptr;
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

size_t DiscardableSharedMemoryHeap::GetFreeListSize() const {
  return num_free_blocks_ * block_size_;
}

scoped_ptr<DiscardableSharedMemoryHeap::Span>
DiscardableSharedMemoryHeap::RemoveFromFreeList(Span* span) {
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
    free_spans_.Append(leftover.release());

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
