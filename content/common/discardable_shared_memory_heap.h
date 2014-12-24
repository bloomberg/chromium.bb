// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DISCARDABLE_SHARED_MEMORY_HEAP_H_
#define CONTENT_COMMON_DISCARDABLE_SHARED_MEMORY_HEAP_H_

#include <bitset>

#include "base/containers/hash_tables.h"
#include "base/containers/linked_list.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"

namespace base {
class DiscardableSharedMemory;
}

namespace content {

// Implements a heap of discardable shared memory. A free list is used to keep
// track of free blocks.
class CONTENT_EXPORT DiscardableSharedMemoryHeap {
 public:
  class CONTENT_EXPORT Span : public base::LinkNode<Span> {
   public:
    ~Span();

    base::DiscardableSharedMemory* shared_memory() {
      return shared_memory_.get();
    }
    size_t start() const { return start_; }
    size_t length() const { return length_; }

   private:
    friend class DiscardableSharedMemoryHeap;

    Span(linked_ptr<base::DiscardableSharedMemory> shared_memory,
         size_t start,
         size_t length);

    linked_ptr<base::DiscardableSharedMemory> shared_memory_;
    size_t start_;
    size_t length_;

    DISALLOW_COPY_AND_ASSIGN(Span);
  };

  explicit DiscardableSharedMemoryHeap(size_t block_size);
  ~DiscardableSharedMemoryHeap();

  // Grow heap using |memory| and return a span for this new memory.
  // |memory| must be aligned to the block size and |size| must be a
  // multiple of the block size.
  scoped_ptr<Span> Grow(scoped_ptr<base::DiscardableSharedMemory> memory,
                        size_t size);

  // Merge |span| into the free list. This will coalesce |span| with
  // neighboring spans in free list when possible.
  void MergeIntoFreeList(scoped_ptr<Span> span);

  // Split an allocated span into two spans, one of length |blocks| followed
  // by another span of length "span->length - blocks" blocks. Modifies |span|
  // to point to the first span of length |blocks|. Return second span.
  scoped_ptr<Span> Split(Span* span, size_t blocks);

  // Search free list for span that satisfies the request for |blocks| of
  // memory. If found, the span is removed from the free list and returned.
  scoped_ptr<Span> SearchFreeList(size_t blocks);

  // Delete span and release memory if possible.
  void DeleteSpan(scoped_ptr<Span> span);

 private:
  scoped_ptr<Span> RemoveFromFreeList(Span* span);
  scoped_ptr<Span> Carve(Span* span, size_t blocks);
  void RegisterSpan(Span* span);

  size_t block_size_;

  // Mapping from first/last block of span to Span instance.
  typedef base::hash_map<size_t, Span*> SpanMap;
  SpanMap spans_;

  // Linked-list of free discardable memory regions.
  base::LinkedList<Span> free_spans_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableSharedMemoryHeap);
};

}  // namespace content

#endif  // CONTENT_COMMON_DISCARDABLE_SHARED_MEMORY_HEAP_H_
