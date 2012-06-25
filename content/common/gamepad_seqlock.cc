// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gamepad_seqlock.h"

namespace content {
namespace internal {

GamepadSeqLockBase::GamepadSeqLockBase(BaseEntry* entries, size_t size)
    : size_(size)
    , current_(0) {
  const size_t ent_size = sizeof(BaseEntry) + sizeof(Atomic32) * (size + 1);
  memset(entries, 0, kEntries * ent_size);
  for (size_t i = 0; i < kEntries; ++i) {
    entries_[i] = reinterpret_cast<BaseEntry*>
        (reinterpret_cast<char*>(entries) + i * ent_size);
  }
}

// The algorithm works as follows.
// The SeqLock contains 2 user objects - a current and a non-current.
// The object roles can be swapped by incrementing the current_ variable.
// Initially both objects are consistent, that is, their sequence_%2 == 0.
// A writer proceeds as follows. First, it marks the non-current object
// as inconsistent by incrementing it's sequence_ (the sequence_ becomes odd).
// Then it mutates the object. Then marks it as consistent by incrementing
// it's sequence_ once again (the sequence_ is even now). And finally swaps
// the object roles, that is, the object becomes current.
// Such disipline establishes an important property - the current object
// is always consistent and contains the most recent data.
// Readers proceed as follows. First, determine what is the current object,
// remember it's seqence, check that the sequence is even
// (the object is consistent), copy out the object, verify that
// the sequence is not changed. If any of the checks fail, a reader works
// with a non-current object (current object is always consistent), so it
// just retries from the beginning. Thus readers are completely lock-free,
// that is, a reader retries iff a writer has accomplished a write operation
// during reading (only constant sequence of writes can stall readers,
// a stalled writer can't block readers).

void GamepadSeqLockBase::ReadTo(Atomic32* obj) {
  using base::subtle::NoBarrier_Load;
  using base::subtle::Acquire_Load;
  using base::subtle::MemoryBarrier;
  for (;;) {
    // Determine the current object.
    Atomic32 cur = Acquire_Load(&current_);
    BaseEntry* ent = entries_[cur % kEntries];
    Atomic32 seq = Acquire_Load(&ent->sequence_);  // membar #LoadLoad
    // If the counter is even, then the object is not already current,
    // no need to yield, just retry (the current object is consistent).
    if (seq % 2)
      continue;
    // Copy out the entry.
    for (size_t i = 0; i < size_; ++i)
      obj[i] = NoBarrier_Load(&ent->data_[i]);
    MemoryBarrier();  // membar #LoadLoad
    Atomic32 seq2 = NoBarrier_Load(&ent->sequence_);
    // If the counter is changed, then we've read inconsistent data,
    // no need to yield, just retry (the current object is consistent).
    if (seq2 != seq)
      continue;
    break;
  }
}

void GamepadSeqLockBase::Write(const Atomic32* obj) {
  using base::subtle::NoBarrier_Store;
  using base::subtle::Release_Store;
  using base::subtle::MemoryBarrier;
  // Get the non current object...
  BaseEntry* ent = entries_[(current_ + 1) % kEntries];
  // ... and mark it as inconsistent.
  NoBarrier_Store(&ent->sequence_, ent->sequence_ + 1);
  MemoryBarrier();  // membar #StoreStore
  // Copy in the entry.
  for (size_t i = 0; i < size_; ++i)
    NoBarrier_Store(&ent->data_[i], obj[i]);
  // Mark the object as consistent again.
  Release_Store(&ent->sequence_, ent->sequence_ + 1);  // membar #StoreStore
  // Switch the current object.
  Release_Store(&current_, current_ + 1);
}

}  // namespace internal
}  // namespace content
