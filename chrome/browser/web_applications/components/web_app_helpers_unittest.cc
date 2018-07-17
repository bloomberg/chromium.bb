// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_helpers.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

TEST(WebAppHelpers, GenerateApplicationNameFromURL) {
  EXPECT_EQ("_", GenerateApplicationNameFromURL(GURL()));

  EXPECT_EQ("example.com_/",
            GenerateApplicationNameFromURL(GURL("http://example.com")));

  EXPECT_EQ("example.com_/path",
            GenerateApplicationNameFromURL(GURL("https://example.com/path")));
}

}  // namespace web_app
