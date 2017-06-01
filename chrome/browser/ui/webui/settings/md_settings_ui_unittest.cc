// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_ui.h"

#include "chrome/common/url_constants.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(MdSettingsUITest, IsValidOrigin) {
  EXPECT_TRUE(settings::IsValidOrigin(GURL(chrome::kChromeUISettingsURL)));
  EXPECT_TRUE(settings::IsValidOrigin(GURL(chrome::kChromeUIMdSettingsURL)));

  EXPECT_FALSE(settings::IsValidOrigin(GURL("http://evil.com")));
  EXPECT_FALSE(settings::IsValidOrigin(GURL("https://google.com")));
  EXPECT_FALSE(settings::IsValidOrigin(GURL(chrome::kChromeUINewTabURL)));
  EXPECT_FALSE(settings::IsValidOrigin(GURL(chrome::kChromeUIHistoryURL)));
}
