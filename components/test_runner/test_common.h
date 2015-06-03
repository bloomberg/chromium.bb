// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_TEST_COMMON_H_
#define COMPONENTS_TEST_RUNNER_TEST_COMMON_H_

#include <string>

namespace test_runner {

inline bool IsASCIIAlpha(char ch) {
  return (ch | 0x20) >= 'a' && (ch | 0x20) <= 'z';
}

inline bool IsNotASCIIAlpha(char ch) {
  return !IsASCIIAlpha(ch);
}

std::string NormalizeLayoutTestURL(const std::string& url);

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_TEST_COMMON_H_
