// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"

#include <limits>

#include "base/bits.h"

namespace base {

LockFreeAddressHashSet::LockFreeAddressHashSet(size_t buckets_count)
    : buckets_(buckets_count), bucket_mask_(buckets_count - 1) {
  DCHECK(bits::IsPowerOfTwo(buckets_count));
  DCHECK_LE(bucket_mask_, std::numeric_limits<uint32_t>::max());
}

LockFreeAddressHashSet::~LockFreeAddressHashSet() {
  for (std::atomic<Node*>& bucket : buckets_) {
    Node* node = bucket.load(std::memory_order_relaxed);
    while (node) {
      Node* next = node->next.load(std::memory_order_relaxed);
      delete node;
      node = next;
    }
  }
}

void LockFreeAddressHashSet::Insert(void* key) {
  DCHECK_NE(key, nullptr);
  CHECK(!Contains(key));
  size_.fetch_add(1, std::memory_order_relaxed);
  std::atomic<Node*>& bucket = buckets_[Hash(key) & bucket_mask_];
  Node* node = bucket.load(std::memory_order_acquire);
  // First iterate over the bucket nodes and try to reuse an empty one if found.
  for (; node != nullptr; node = next_node(node)) {
    void* empty_key = nullptr;
    bool replaced = node->key.compare_exchange_strong(
        empty_key, key, std::memory_order_relaxed);
    if (replaced)
      return;
  }
  DCHECK_EQ(node, nullptr);
  // There are no empty nodes to reuse in the bucket.
  // Create a new node and prepend it to the list.
  Node* new_node = new Node(key);
  Node* current_head = bucket.load(std::memory_order_acquire);
  do {
    new_node->next.store(current_head, std::memory_order_relaxed);
  } while (!bucket.compare_exchange_weak(current_head, new_node,
                                         std::memory_order_release));
}

void LockFreeAddressHashSet::Copy(const LockFreeAddressHashSet& other) {
  DCHECK_EQ(0u, size());
  for (const std::atomic<Node*>& bucket : other.buckets_) {
    for (Node* node = bucket.load(std::memory_order_acquire); node;
         node = next_node(node)) {
      void* key = node->key.load(std::memory_order_relaxed);
      if (key)
        Insert(key);
    }
  }
}

}  // namespace base
