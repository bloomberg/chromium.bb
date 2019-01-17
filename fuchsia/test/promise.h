// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_TEST_PROMISE_H_
#define FUCHSIA_TEST_PROMISE_H_

#include "base/bind_helpers.h"
#include "base/optional.h"

namespace webrunner {

// Stores an asynchronously generated value for later retrieval, optionally
// invoking a callback on value receipt for controlling test flow.
//
// The value can be read by using the dereference (*) or arrow (->) operators.
// Values must first be received before they can be accessed. Dereferencing a
// value before it is set will produce a CHECK violation.
template <typename T>
class Promise {
 public:
  explicit Promise(base::RepeatingClosure on_capture = base::DoNothing())
      : on_capture_(std::move(on_capture)) {}

  Promise(Promise&& other) = default;

  ~Promise() = default;

  // Returns a OnceCallback which will receive and store a value T.
  base::OnceCallback<void(T)> GetReceiveCallback() {
    return base::BindOnce(&Promise<T>::ReceiveValue, base::Unretained(this));
  }

  void ReceiveValue(T value) {
    captured_ = std::move(value);
    on_capture_.Run();
  }

  bool has_value() const { return captured_.has_value(); }

  T& operator*() {
    CHECK(captured_.has_value());
    return *captured_;
  }

  T* operator->() {
    CHECK(captured_.has_value());
    return &*captured_;
  }

 private:
  base::Optional<T> captured_;
  base::RepeatingClosure on_capture_;

  DISALLOW_COPY_AND_ASSIGN(Promise<T>);
};

// Converts a Chromium OnceCallback to a fit::function.
template <typename TRet, typename... TArgs>
fit::function<TRet(TArgs...)> ConvertToFitFunction(
    base::OnceCallback<TRet(TArgs...)> callback) {
  return [callback = std::move(callback)](TArgs... args) mutable {
    std::move(callback).Run(std::forward<TArgs>(args)...);
  };
}

}  // namespace webrunner

#endif  // FUCHSIA_TEST_PROMISE_H_