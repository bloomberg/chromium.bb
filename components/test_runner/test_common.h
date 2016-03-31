// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_TEST_COMMON_H_
#define COMPONENTS_TEST_RUNNER_TEST_COMMON_H_

#include <string>

#include "components/test_runner/test_runner_export.h"

namespace test_runner {

inline bool IsASCIIAlpha(char ch) {
  return (ch | 0x20) >= 'a' && (ch | 0x20) <= 'z';
}

inline bool IsNotASCIIAlpha(char ch) {
  return !IsASCIIAlpha(ch);
}

TEST_RUNNER_EXPORT std::string NormalizeLayoutTestURL(const std::string& url);

// Tests which depend on Blink must call this function on the main thread
// before creating/calling any Blink objects/APIs.
TEST_RUNNER_EXPORT void EnsureBlinkInitialized();

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_TEST_COMMON_H_
