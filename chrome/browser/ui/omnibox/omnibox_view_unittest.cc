// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(OmniboxView, TestStripSchemasUnsafeForPaste) {
  const char* urls[] = {
    "http://www.google.com?q=javascript:alert(0)",  // Safe URL.
    "javAscript:alert(0)",                          // Unsafe JS URL.
    "jaVascript:\njavaScript: alert(0)"             // Single strip unsafe.
  };

  const char* expecteds[] = {
    "http://www.google.com?q=javascript:alert(0)",  // Safe URL.
    "alert(0)",                                     // Unsafe JS URL.
    "alert(0)"                                      // Single strip unsafe.
  };

  for (size_t i = 0; i < arraysize(urls); i++) {
    EXPECT_EQ(ASCIIToUTF16(expecteds[i]),
              OmniboxView::StripJavascriptSchemas(ASCIIToUTF16(urls[i])));
  }
}
