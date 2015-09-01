// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::ASCIIToUTF16;

namespace {

class OmniboxViewTest : public PlatformTest {
};

TEST_F(OmniboxViewTest, TestStripSchemasUnsafeForPaste) {
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

TEST_F(OmniboxViewTest, SanitizeTextForPaste) {
  // Broken URL has newlines stripped.
  const base::string16 kWrappedURL(ASCIIToUTF16(
      "http://www.chromium.org/developers/testing/chromium-\n"
      "build-infrastructure/tour-of-the-chromium-buildbot"));

  const base::string16 kFixedURL(ASCIIToUTF16(
      "http://www.chromium.org/developers/testing/chromium-"
      "build-infrastructure/tour-of-the-chromium-buildbot"));
  EXPECT_EQ(kFixedURL, OmniboxView::SanitizeTextForPaste(kWrappedURL));

  // Multi-line address is converted to a single-line address.
  const base::string16 kWrappedAddress(ASCIIToUTF16(
      "1600 Amphitheatre Parkway\nMountain View, CA"));

  const base::string16 kFixedAddress(ASCIIToUTF16(
      "1600 Amphitheatre Parkway Mountain View, CA"));
  EXPECT_EQ(kFixedAddress, OmniboxView::SanitizeTextForPaste(kWrappedAddress));
}

}  // namespace
