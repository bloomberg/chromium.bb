// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_DEBUG_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_DEBUG_UTIL_H_

namespace mojo {
namespace internal {

// TODO(crbug.com/741047): Remove this after the bug is fixed.
class LifeTimeTrackerForDebugging {
 public:
  LifeTimeTrackerForDebugging() {}

  ~LifeTimeTrackerForDebugging() { state_ = DESTROYED; }

  void CheckObjectIsValid() {
    CHECK(this);

    // This adress has been used to construct a LifeTimeTrackerForDebugging.
    CHECK(state_ == ALIVE || state_ == DESTROYED);

    // If the previous check succeeds but this fails, it is likely to be
    // use-after-free problem.
    CHECK_EQ(ALIVE, state_);
  }

 private:
  enum MagicValue : uint64_t {
    ALIVE = 0x1029384756afbecd,
    DESTROYED = 0xdcebfa6574839201
  };

  uint64_t state_ = ALIVE;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_DEBUG_UTIL_H_
