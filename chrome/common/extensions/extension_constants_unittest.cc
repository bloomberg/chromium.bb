// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_constants.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(ExtensionConstantsTest, IntentSearchQuery) {
  const char expected_url[] = "https://chrome.google.com/webstore/"
      "category/collection/webintent_apps"
      "?_wi=http%3A%2F%2Fwebintents.org%2Fedit&_mt=image%2Fjpeg";

  GURL result = extension_urls::GetWebstoreIntentQueryURL(
      "http://webintents.org/edit", "image/jpeg");
   EXPECT_EQ(expected_url, result.spec());
}
