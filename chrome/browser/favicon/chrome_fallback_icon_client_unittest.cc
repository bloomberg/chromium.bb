// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/chrome_fallback_icon_client.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeFallbackIconClientTest, GetFontNameList) {
  ChromeFallbackIconClient client;
  // Just ensure non-empty, otherwise not checking the actual font.
  EXPECT_FALSE(client.GetFontNameList().empty());
}
