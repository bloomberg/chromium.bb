// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome_frame/html_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(HttpUtils, HasFrameBustingHeader) {
  // Simple negative cases.
  ASSERT_FALSE(http_utils::HasFrameBustingHeader(""));
  ASSERT_FALSE(http_utils::HasFrameBustingHeader("Content-Type: text/plain"));
  // Explicit negative cases, test that we ignore case.
  ASSERT_FALSE(http_utils::HasFrameBustingHeader("X-Frame-Options: ALLOWALL"));
  ASSERT_FALSE(http_utils::HasFrameBustingHeader("X-Frame-Options: allowall"));
  ASSERT_FALSE(http_utils::HasFrameBustingHeader("X-Frame-Options: ALLowalL"));
  // Added space, ensure stripped out
  ASSERT_FALSE(http_utils::HasFrameBustingHeader(
      "X-Frame-Options: ALLOWALL "));
  // Added space with linefeed, ensure still stripped out
  ASSERT_FALSE(http_utils::HasFrameBustingHeader(
      "X-Frame-Options: ALLOWALL \r\n"));
  // Multiple identical headers, all of them allowing framing.
  ASSERT_FALSE(http_utils::HasFrameBustingHeader(
      "X-Frame-Options: ALLOWALL\r\n"
      "X-Frame-Options: ALLOWALL\r\n"
      "X-Frame-Options: ALLOWALL"));
  // Interleave with other headers.
  ASSERT_FALSE(http_utils::HasFrameBustingHeader(
      "Content-Type: text/plain\r\n"
      "X-Frame-Options: ALLOWALL\r\n"
      "Content-Length: 42"));

  // Simple positive cases.
  ASSERT_TRUE(http_utils::HasFrameBustingHeader("X-Frame-Options: deny"));
  ASSERT_TRUE(http_utils::HasFrameBustingHeader(
      "X-Frame-Options: SAMEorigin"));

  // Allowall entries do not override the denying entries, are
  // order-independent, and the deny entries can interleave with
  // other headers.
  ASSERT_TRUE(http_utils::HasFrameBustingHeader(
      "Content-Length: 42\r\n"
      "X-Frame-Options: ALLOWall\r\n"
      "X-Frame-Options: deny\r\n"));
  ASSERT_TRUE(http_utils::HasFrameBustingHeader(
      "X-Frame-Options: ALLOWall\r\n"
      "Content-Length: 42\r\n"
      "X-Frame-Options: SAMEORIGIN\r\n"));
  ASSERT_TRUE(http_utils::HasFrameBustingHeader(
      "X-Frame-Options: deny\r\n"
      "X-Frame-Options: ALLOWall\r\n"
      "Content-Length: 42\r\n"));
  ASSERT_TRUE(http_utils::HasFrameBustingHeader(
      "X-Frame-Options: SAMEORIGIN\r\n"
      "X-Frame-Options: ALLOWall\r\n"));
}

}  // namespace
