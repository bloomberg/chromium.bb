// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GAMEPAD_SEQLOCK_H_
#define CONTENT_COMMON_GAMEPAD_SEQLOCK_H_

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace content {

namespace internal {

class CONTENT_EXPORT GamepadSeqLockBase {
 protected:
  static const size_t kEntries = 2;
  typedef base::subtle::Atomic32 Atomic32;
  struct BaseEntry {
    Atomic32 sequence_;
    // Both writer and readers work with the array concurrently,
    // so it must be accessed with atomic operations.
    Atomic32 data_[1];
  };

  GamepadSeqLockBase(BaseEntry* entries, size_t size);
  void ReadTo(Atomic32* obj);
  void Write(const Atomic32* obj);

 private:
  const size_t size_;
  Atomic32 current_;
  BaseEntry* entries_[kEntries];
};

}  // namespace internal

// This SeqLock handles only *one* writer and multiple readers. It may be
// suitable for high read frequency scenarios and is especially useful in shared
// memory environment. See for the basic idea:
//   http://en.wikipedia.org/wiki/Seqlock
// However, this implementation is an improved lock-free variant.
// The SeqLock can hold only POD fully-embed data (no pointers
// to satellite data), copies are done with memcpy.
template<typename T>
class GamepadSeqLock : public internal::GamepadSeqLockBase {
 public:
  GamepadSeqLock()
      : internal::GamepadSeqLockBase(entries_, kSize) {
  }

  // May be called concurrently with ReadTo and Write.
  // - If ReadTo occurs in the middle of Write (on another thread),
  //   ReadTo gets the old data (not the new data being written
  //   by the other thread).
  // - ReadTo never spins unless the writer thread is calling Write
  //   multiple times before ReadTo completes.
  void ReadTo(T* obj) {
    // Breaks strict-aliasing rules.
    Base::ReadTo(reinterpret_cast<Atomic32*>(obj));
  }

  // Must be called by a single thread at a time.  May be called concurrently
  // with ReadTo.
  void Write(const T& obj) {
    // Breaks strict-aliasing rules.
    Base::Write(reinterpret_cast<const Atomic32*>(&obj));
  }

private:
  typedef internal::GamepadSeqLockBase Base;
  COMPILE_ASSERT(sizeof(T) % sizeof(Atomic32) == 0, bad_object_size);
  static size_t const kSize = sizeof(T) / sizeof(Atomic32);
  struct Entry : Base::BaseEntry {
    Atomic32 data_[kSize];
  };
  Entry entries_[kEntries];
  DISALLOW_COPY_AND_ASSIGN(GamepadSeqLock);
};

}  // namespace content

#endif  // CONTENT_COMMON_GAMEPAD_SEQLOCK_H_
