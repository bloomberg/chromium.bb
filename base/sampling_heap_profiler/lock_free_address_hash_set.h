// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SAMPLING_HEAP_PROFILER_LOCK_FREE_ADDRESS_HASH_SET_H_
#define BASE_SAMPLING_HEAP_PROFILER_LOCK_FREE_ADDRESS_HASH_SET_H_

#include <atomic>
#include <cstdint>
#include <vector>

#include "base/logging.h"

namespace base {

// A hash set container that provides lock-free versions of
// |Insert|, |Remove|, and |Contains| operations.
// It does not support concurrent write operations |Insert| and |Remove|
// over the same key. Concurrent writes of distinct keys are ok.
// |Contains| method can be executed concurrently with other |Insert|, |Remove|,
// or |Contains| even over the same key.
// However, please note the result of concurrent execution of |Contains|
// with |Insert| or |Remove| is racy.
//
// The hash set never rehashes, so the number of buckets stays the same
// for the lifetime of the set.
//
// Internally the hashset is implemented as a vector of N buckets
// (N has to be a power of 2). Each bucket holds a single-linked list of
// nodes each corresponding to a key.
// It is not possible to really delete nodes from the list as there might
// be concurrent reads being executed over the node. The |Remove| operation
// just marks the node as empty by placing nullptr into its key field.
// Consequent |Insert| operations may reuse empty nodes when possible.
//
// The structure of the hashset for N buckets is the following:
// 0: {*}--> {key1,*}--> {key2,*}--> NULL
// 1: {*}--> NULL
// 2: {*}--> {NULL,*}--> {key3,*}--> {key4,*}--> NULL
// ...
// N-1: {*}--> {keyM,*}--> NULL
class BASE_EXPORT LockFreeAddressHashSet {
 public:
  explicit LockFreeAddressHashSet(size_t buckets_count);
  ~LockFreeAddressHashSet();

  // Checks if the |key| is in the set. Can be executed concurrently with
  // |Insert|, |Remove|, and |Contains| operations.
  bool Contains(void* key) const;

  // Removes the |key| from the set. The key must be present in the set before
  // the invocation.
  // Can be concurrent with other |Insert| and |Remove| executions, provided
  // they operate over distinct keys.
  // Concurrent |Insert| or |Remove| executions over the same key are not
  // supported.
  void Remove(void* key);

  // Inserts the |key| into the set. The key must not be present in the set
  // before the invocation.
  // Can be concurrent with other |Insert| and |Remove| executions, provided
  // they operate over distinct keys.
  // Concurrent |Insert| or |Remove| executions over the same key are not
  // supported.
  void Insert(void* key);

  // Copies contents of |other| set into the current set. The current set
  // must be empty before the call.
  // The operation cannot be executed concurrently with any other methods.
  void Copy(const LockFreeAddressHashSet& other);

  size_t buckets_count() const { return buckets_.size(); }
  size_t size() const { return size_.load(std::memory_order_relaxed); }

  // Returns the average bucket utilization.
  float load_factor() const { return 1.f * size() / buckets_.size(); }

 private:
  friend class LockFreeAddressHashSetTest;

  struct Node {
    explicit Node(void* key);
    std::atomic<void*> key;
    std::atomic<Node*> next;
  };

  static uint32_t Hash(void* key);
  Node* FindNode(void* key) const;
  static Node* next_node(Node* node) {
    return node->next.load(std::memory_order_acquire);
  }

  std::vector<std::atomic<Node*>> buckets_;
  std::atomic_int size_{0};
  const size_t bucket_mask_;
};

inline LockFreeAddressHashSet::Node::Node(void* a_key) {
  key.store(a_key, std::memory_order_relaxed);
  // |next| field is initiailized later, right before the node is inserted
  // into the linked list.
}

inline bool LockFreeAddressHashSet::Contains(void* key) const {
  return FindNode(key) != nullptr;
}

inline void LockFreeAddressHashSet::Remove(void* key) {
  Node* node = FindNode(key);
  DCHECK_NE(node, nullptr);
  // We can never delete the node, nor detach it from the current bucket
  // as there may always be another thread currently iterating over it.
  // Instead we just mark it as empty, so |Insert| can reuse it later.
  node->key.store(nullptr, std::memory_order_relaxed);
  size_.fetch_add(-1, std::memory_order_relaxed);
}

inline LockFreeAddressHashSet::Node* LockFreeAddressHashSet::FindNode(
    void* key) const {
  DCHECK_NE(key, nullptr);
  const std::atomic<Node*>& bucket = buckets_[Hash(key) & bucket_mask_];
  for (Node* node = bucket.load(std::memory_order_acquire); node != nullptr;
       node = next_node(node)) {
    void* k = node->key.load(std::memory_order_relaxed);
    if (k == key)
      return node;
  }
  return nullptr;
}

// static
inline uint32_t LockFreeAddressHashSet::Hash(void* key) {
  // A simple fast hash function for addresses.
  uint64_t k = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(key));
  uint64_t random_bits = 0x4bfdb9df5a6f243bull;
  return static_cast<uint32_t>((k * random_bits) >> 32);
}

}  // namespace base

#endif  // BASE_SAMPLING_HEAP_PROFILER_LOCK_FREE_ADDRESS_HASH_SET_H_
