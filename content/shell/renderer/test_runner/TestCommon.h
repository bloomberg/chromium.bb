// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_TESTCOMMON_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_TESTCOMMON_H_

#include <stdio.h>
#include <string>

#include "base/compiler_specific.h"

#if defined(WIN32)
#define snprintf(str, size, ...) _snprintf_s(str, size, size, __VA_ARGS__)
#endif

namespace content {

inline bool isASCIIAlpha(char ch) { return (ch | 0x20) >= 'a' && (ch | 0x20) <= 'z'; }

inline bool isNotASCIIAlpha(char ch) { return !isASCIIAlpha(ch); }

std::string normalizeLayoutTestURL(const std::string& url);

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_TESTCOMMON_H_
