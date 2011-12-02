// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/content_settings_observer.h"

#include "chrome/common/url_constants.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"

using WebKit::WebSecurityOrigin;

typedef testing::Test ContentSettingsObserverTest;

TEST_F(ContentSettingsObserverTest, WhitelistedSchemes) {
  std::string end_url = ":something";

  GURL chrome_ui_url =
      GURL(std::string(chrome::kChromeUIScheme).append(end_url));
  EXPECT_TRUE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(chrome_ui_url),
      GURL()));

  GURL chrome_dev_tools_url =
      GURL(std::string(chrome::kChromeDevToolsScheme).append(end_url));
  EXPECT_TRUE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(chrome_dev_tools_url),
      GURL()));

  GURL extension_url =
      GURL(std::string(chrome::kExtensionScheme).append(end_url));
  EXPECT_TRUE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(extension_url),
      GURL()));

  GURL chrome_internal_url =
      GURL(std::string(chrome::kChromeInternalScheme).append(end_url));
  EXPECT_TRUE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(chrome_internal_url),
      GURL()));

  GURL file_url("file:///dir/");
  EXPECT_TRUE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(file_url),
      GURL("file:///dir/")));
  EXPECT_FALSE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(file_url),
      GURL("file:///dir/file")));

  GURL ftp_url("ftp:///dir/");
  EXPECT_TRUE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(ftp_url),
      GURL("ftp:///dir/")));
  EXPECT_FALSE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(ftp_url),
      GURL("ftp:///dir/file")));

  GURL http_url =
      GURL("http://server.com/path");
  EXPECT_FALSE(ContentSettingsObserver::IsWhitelistedForContentSettings(
      WebSecurityOrigin::create(http_url),
      GURL()));
}
