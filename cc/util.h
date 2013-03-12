// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_UTIL_H_
#define CC_UTIL_H_

namespace cc {

template <typename T> T RoundUp(T n, T mul) {
  return ((n + mul - 1) / mul) * mul;
}

}  // namespace cc

#endif  // CC_UTIL_H_
