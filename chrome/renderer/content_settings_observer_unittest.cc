// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/content_settings_observer.h"

#include "chrome/common/url_constants.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "url/gurl.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

using blink::WebSecurityOrigin;

typedef testing::Test ContentSettingsObserverTest;

TEST_F(ContentSettingsObserverTest, WhitelistedSchemes) {
  std::string end_url = ":something";

  GURL chrome_ui_url =
      GURL(std::string(content::kChromeUIScheme).append(end_url));
  EXPECT_TRUE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(chrome_ui_url),
      GURL()));

  GURL chrome_dev_tools_url =
      GURL(std::string(content::kChromeDevToolsScheme).append(end_url));
  EXPECT_TRUE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(chrome_dev_tools_url),
      GURL()));

#if defined(ENABLE_EXTENSIONS)
  GURL extension_url =
      GURL(std::string(extensions::kExtensionScheme).append(end_url));
  EXPECT_TRUE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(extension_url),
      GURL()));
#endif

  GURL file_url("file:///dir/");
  EXPECT_TRUE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(file_url),
      GURL("file:///dir/")));
  EXPECT_FALSE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(file_url),
      GURL("file:///dir/file")));

  GURL http_url =
      GURL("http://server.com/path");
  EXPECT_FALSE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(http_url),
      GURL()));
}
