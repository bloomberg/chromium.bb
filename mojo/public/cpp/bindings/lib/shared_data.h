// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SHARED_DATA_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SHARED_DATA_H_

#include <assert.h>

#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace internal {

// Used to allocate an instance of T that can be shared via reference counting.
//
// A default constructed SharedData does not have a Holder until it is set,
// either by assignment, or by accessing the value. As a result, it is not tied
// to any thread. The Holder is lazily allocated on first access. The complexity
// is due to the behaviour around copying. If a default constructed SharedData
// is copied into another, the two share the same empty state, and changing the
// value of one will affect the other.
template <typename T>
class SharedData {
 public:
  ~SharedData() {
    if (holder_)
      holder_->Release();
  }

  SharedData() : holder_(nullptr) {}

  explicit SharedData(const T& value) : holder_(new Holder(value)) {}

  SharedData(const SharedData<T>& other) {
    other.LazyInit();
    holder_ = other.holder_;
    holder_->Retain();
  }

  SharedData<T>& operator=(const SharedData<T>& other) {
    other.LazyInit();
    if (other.holder_ == holder_)
      return *this;
    if (holder_)
      holder_->Release();
    holder_ = other.holder_;
    holder_->Retain();
    return *this;
  }

  void reset() {
    if (holder_)
      holder_->Release();
    holder_ = nullptr;
  }

  void reset(const T& value) {
    if (holder_)
      holder_->Release();
    holder_ = new Holder(value);
  }

  void set_value(const T& value) {
    LazyInit();
    holder_->value = value;
  }
  T* mutable_value() {
    LazyInit();
    return &holder_->value;
  }
  const T& value() const {
    LazyInit();
    return holder_->value;
  }

 private:
  class Holder {
   public:
    Holder() : value(), ref_count_(1) {}
    Holder(const T& value) : value(value), ref_count_(1) {}

    void Retain() {
      assert(thread_checker_.CalledOnValidThread());
      ++ref_count_;
    }
    void Release() {
      assert(thread_checker_.CalledOnValidThread());
      if (--ref_count_ == 0)
        delete this;
    }

    T value;

   private:
    int ref_count_;
    base::ThreadChecker thread_checker_;
    MOJO_DISALLOW_COPY_AND_ASSIGN(Holder);
  };

  void LazyInit() const {
    if (!holder_)
      holder_ = new Holder();
  }

  mutable Holder* holder_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SHARED_DATA_H_
