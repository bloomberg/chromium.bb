// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_ATOMIC_BOOLEAN_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_ATOMIC_BOOLEAN_H_

namespace location {
namespace nearby {

// A boolean value that may be updated atomically.
//
// https://docs.oracle.com/javase/7/docs/api/java/util/concurrent/atomic/AtomicBoolean.html
class AtomicBoolean {
 public:
  virtual ~AtomicBoolean() {}

  virtual bool get() = 0;
  virtual void set(bool value) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_ATOMIC_BOOLEAN_H__
