// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_COMMON_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_COMMON_H_

#include <string>

namespace content {

inline bool IsASCIIAlpha(char ch) {
  return (ch | 0x20) >= 'a' && (ch | 0x20) <= 'z';
}

inline bool IsNotASCIIAlpha(char ch) {
  return !IsASCIIAlpha(ch);
}

std::string NormalizeLayoutTestURL(const std::string& url);

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_COMMON_H_
