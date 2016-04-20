// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_MAYBE_H_
#define HEADLESS_PUBLIC_UTIL_MAYBE_H_

#include <algorithm>

#include "base/logging.h"
#include "base/macros.h"

namespace headless {

// A simple Maybe which may or may not have a value. Based on v8::Maybe.
// TODO(skyostil): Replace this with base::Optional once it is available.
template <typename T>
class Maybe {
 public:
  Maybe() : has_value_(false) {}

  bool IsNothing() const { return !has_value_; }
  bool IsJust() const { return has_value_; }

  // Will crash if the Maybe<> is nothing.
  T& FromJust() {
    DCHECK(IsJust());
    return value_;
  }
  const T& FromJust() const {
    DCHECK(IsJust());
    return value_;
  }

  T FromMaybe(const T& default_value) const {
    return has_value_ ? value_ : default_value;
  }

  bool operator==(const Maybe& other) const {
    return (IsJust() == other.IsJust()) &&
           (!IsJust() || FromJust() == other.FromJust());
  }

  bool operator!=(const Maybe& other) const { return !operator==(other); }

  Maybe& operator=(Maybe&& other) {
    has_value_ = other.has_value_;
    value_ = std::move(other.value_);
    return *this;
  }

  Maybe& operator=(const Maybe& other) {
    has_value_ = other.has_value_;
    value_ = other.value_;
    return *this;
  }

  Maybe(const Maybe& other) = default;
  Maybe(Maybe&& other) = default;

 private:
  template <class U>
  friend Maybe<U> Nothing();
  template <class U>
  friend Maybe<U> Just(const U& u);
  template <class U>
  friend Maybe<typename std::remove_reference<U>::type> Just(U&& u);

  explicit Maybe(const T& t) : has_value_(true), value_(t) {}
  explicit Maybe(T&& t) : has_value_(true), value_(std::move(t)) {}

  bool has_value_;
  T value_;
};

template <class T>
Maybe<T> Nothing() {
  return Maybe<T>();
}

template <class T>
Maybe<T> Just(const T& t) {
  return Maybe<T>(t);
}

template <class T>
Maybe<typename std::remove_reference<T>::type> Just(T&& t) {
  return Maybe<typename std::remove_reference<T>::type>(std::move(t));
}

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_MAYBE_H_
