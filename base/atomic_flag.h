// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ATOMIC_FLAG_H_
#define BASE_ATOMIC_FLAG_H_

#include "base/atomicops.h"

namespace base {

// AtomicFlag allows threads to notify each other for a single occurrence
// of a single event. It maintains an abstract boolean "flag" that transitions
// to true at most once. It provides calls to query the boolean.
//
// Memory ordering: For any threads X and Y, if X calls Set(), then any
// action taken by X before it calls Set() is visible to thread Y after
// Y receives a true return value from IsSet().
class AtomicFlag {
 public:
  // Sets "flag_" to "initial_value".
  explicit AtomicFlag(bool initial_value = false)
      : flag_(initial_value ? 1 : 0) { }
  ~AtomicFlag() {}

  void Set();  // Set "flag_" to true. May be called only once.
  bool IsSet() const;  // Return "flag_".

 private:
  base::subtle::Atomic32 flag_;

  DISALLOW_COPY_AND_ASSIGN(AtomicFlag);
};

}  // namespace base

#endif  // BASE_ATOMIC_FLAG_H_
