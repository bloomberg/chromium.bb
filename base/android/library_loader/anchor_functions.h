// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_LIBRARY_LOADER_ANCHOR_FUNCTIONS_H_
#define BASE_ANDROID_LIBRARY_LOADER_ANCHOR_FUNCTIONS_H_

#include <cstdint>

namespace base {
namespace android {

// Start and end of .text, respectively.
extern const size_t kStartOfText;
extern const size_t kEndOfText;

// Basic CHECK()s ensuring that the symbols above are correctly set.
void CheckOrderingSanity();

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_LIBRARY_LOADER_ANCHOR_FUNCTIONS_H_
