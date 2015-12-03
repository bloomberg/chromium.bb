// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_UTIL_CAPTURE_UTIL_H_
#define MOJO_UTIL_CAPTURE_UTIL_H_

#include <utility>

#include "mojo/public/cpp/bindings/callback.h"

namespace mojo {

template <typename T1>
mojo::Callback<void(T1)> Capture(T1* t1) {
  return [t1](T1 got_t1) { *t1 = std::move(got_t1); };
}

template <typename T1, typename T2>
mojo::Callback<void(T1, T2)> Capture(T1* t1, T2* t2) {
  return [t1, t2](T1 got_t1, T2 got_t2) {
    *t1 = std::move(got_t1);
    *t2 = std::move(got_t2);
  };
}

template <typename T1, typename T2, typename T3>
mojo::Callback<void(T1, T2, T3)> Capture(T1* t1, T2* t2, T3* t3) {
  return [t1, t2, t3](T1 got_t1, T2 got_t2, T3 got_t3) {
    *t1 = std::move(got_t1);
    *t2 = std::move(got_t2);
    *t3 = std::move(got_t3);
  };
}

}  // namespace mojo

#endif  // MOJO_UTIL_CAPTURE_UTIL_H_
