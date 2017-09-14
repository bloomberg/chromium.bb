// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TRACKED_SCOPED_REFPTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TRACKED_SCOPED_REFPTR_H_

#include "base/debug/alias.h"

namespace mojo {

// TODO(crbug.com/750267, crbug.com/754946): Remove this after bug
// investigation.
template <typename T>
class TrackedScopedRefPtr {
 public:
  TrackedScopedRefPtr() {}

  TrackedScopedRefPtr(T* ptr) : ptr_(ptr) {
    null_reason_ = ptr_ ? NOT_NULL : INITIALIZED_BY_ASSIGN;
  }

  TrackedScopedRefPtr(const TrackedScopedRefPtr<T>& other) : ptr_(other.ptr_) {
    null_reason_ = ptr_ ? NOT_NULL : INITIALIZED_BY_ASSIGN;
  }

  TrackedScopedRefPtr(TrackedScopedRefPtr<T>&& other)
      : ptr_(std::move(other.ptr_)) {
    null_reason_ = ptr_ ? NOT_NULL : INITIALIZED_BY_MOVE;

    if (ptr_)
      other.null_reason_ = MOVED_OUT;
  }

  ~TrackedScopedRefPtr() { state_ = DESTROYED; }

  T* get() const { return ptr_.get(); }

  T& operator*() const { return *ptr_; }

  T* operator->() const { return ptr_.get(); }

  TrackedScopedRefPtr<T>& operator=(T* ptr) {
    ptr_ = ptr;
    null_reason_ = ptr_ ? NOT_NULL : ASSIGNED;
    return *this;
  }

  TrackedScopedRefPtr<T>& operator=(const TrackedScopedRefPtr<T>& other) {
    ptr_ = other.ptr_;
    null_reason_ = ptr_ ? NOT_NULL : ASSIGNED;
    return *this;
  }

  TrackedScopedRefPtr<T>& operator=(TrackedScopedRefPtr<T>&& other) {
    ptr_ = std::move(other.ptr_);
    null_reason_ = ptr_ ? NOT_NULL : MOVED_IN;
    if (ptr_)
      other.null_reason_ = MOVED_OUT;
    return *this;
  }

  void CheckObjectIsValid() {
    CHECK(this);

    LifeState state = state_;
    base::debug::Alias(&state);

    NullReason null_reason = null_reason_;
    base::debug::Alias(&null_reason);

    // This address has been used to construct a TrackedScopedRefPtr.
    CHECK(state_ == ALIVE || state_ == DESTROYED);

    // If the previous check succeeds but this fails, it is likely to be
    // use-after-free problem.
    CHECK_EQ(ALIVE, state_);

    CHECK_NE(DEFAULT_INITIALIZED, null_reason_);
    CHECK_NE(INITIALIZED_BY_ASSIGN, null_reason_);
    CHECK_NE(INITIALIZED_BY_MOVE, null_reason_);
    CHECK_NE(ASSIGNED, null_reason_);
    CHECK_NE(MOVED_IN, null_reason_);
    CHECK_NE(MOVED_OUT, null_reason_);

    CHECK_EQ(NOT_NULL, null_reason_);
    CHECK(ptr_);
  }

 private:
  enum LifeState : uint64_t {
    ALIVE = 0x1029384756afbecd,
    DESTROYED = 0xdcebfa6574839201
  };

  enum NullReason : uint64_t {
    DEFAULT_INITIALIZED = 0x1234432112344321,
    INITIALIZED_BY_ASSIGN = 0x3456654334566543,
    INITIALIZED_BY_MOVE = 0x5678876556788765,
    ASSIGNED = 0x789a9a87789a9a87,
    MOVED_IN = 0x9abccba99abccba9,
    MOVED_OUT = 0xbcdeedcbbcdeedcb,
    NOT_NULL = 0xcdeffedccdeffedc
  };

  LifeState state_ = ALIVE;
  scoped_refptr<T> ptr_;
  NullReason null_reason_ = DEFAULT_INITIALIZED;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TRACKED_SCOPED_REFPTR_H_
