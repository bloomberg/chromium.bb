// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PASSABLE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PASSABLE_H_

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

template <typename HandleType>
class Passable {
 public:
  // |ptr| may be null.
  explicit Passable(HandleType* ptr) : ptr_(ptr) {
  }

  bool is_valid() const {
    return ptr_ && ptr_->is_valid();
  }

  HandleType get() const {
    return ptr_ ? *ptr_ : HandleType();
  }

  HandleType release() MOJO_WARN_UNUSED_RESULT {
    return ptr_ ? internal::FetchAndReset(ptr_) : HandleType();
  }

  ScopedHandleBase<HandleType> Pass() {
    return ScopedHandleBase<HandleType>(release());
  }

 protected:
  Passable();
  // The default copy-ctor and operator= are OK.

  HandleType* ptr_;
};

template <typename HandleType>
inline Passable<HandleType> MakePassable(HandleType* ptr) {
  return Passable<HandleType>(ptr);
}

template <typename HandleType>
class AssignableAndPassable : public Passable<HandleType> {
 public:
  explicit AssignableAndPassable(HandleType* ptr) : Passable<HandleType>(ptr) {
    assert(ptr);
  }

  void operator=(ScopedHandleBase<HandleType> scoper) {
    reset(scoper.release());
  }

  void reset(HandleType obj = HandleType()) {
    ScopedHandleBase<HandleType>(*this->ptr_);
    this->ptr_->set_value(obj.value());
  }
};

template <typename HandleType>
inline AssignableAndPassable<HandleType> MakeAssignableAndPassable(
    HandleType* ptr) {
  return AssignableAndPassable<HandleType>(ptr);
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PASSABLE_H_
