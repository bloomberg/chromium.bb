// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

typedef testing::Test BrowserAboutHandlerTest;

TEST_F(BrowserAboutHandlerTest, WillHandleBrowserAboutURL) {
  std::string chrome_prefix(content::kChromeUIScheme);
  chrome_prefix.append(url::kStandardSchemeSeparator);
  struct AboutURLTestData {
    GURL test_url;
    GURL result_url;
  } test_data[] = {
      {
        GURL("http://google.com"),
        GURL("http://google.com")
      },
      {
        GURL(url::kAboutBlankURL),
        GURL(url::kAboutBlankURL)
      },
      {
        GURL(chrome_prefix + chrome::kChromeUIMemoryHost),
        GURL(chrome_prefix + chrome::kChromeUIMemoryHost)
      },
      {
        GURL(chrome_prefix + chrome::kChromeUIDefaultHost),
        GURL(chrome_prefix + chrome::kChromeUIVersionHost)
      },
      {
        GURL(chrome_prefix + chrome::kChromeUIAboutHost),
        GURL(chrome_prefix + chrome::kChromeUIChromeURLsHost)
      },
      {
        GURL(chrome_prefix + chrome::kChromeUICacheHost),
        GURL(chrome_prefix + content::kChromeUINetworkViewCacheHost)
      },
      {
        GURL(chrome_prefix + chrome::kChromeUISignInInternalsHost),
        GURL(chrome_prefix + chrome::kChromeUISignInInternalsHost)
      },
      {
        GURL(chrome_prefix + chrome::kChromeUISyncHost),
        GURL(chrome_prefix + chrome::kChromeUISyncInternalsHost)
      },
      {
        GURL(chrome_prefix + chrome::kChromeUIInvalidationsHost),
        GURL(chrome_prefix + chrome::kChromeUIInvalidationsHost)
      },
      {
        GURL(chrome_prefix + "host/path?query#ref"),
        GURL(chrome_prefix + "host/path?query#ref"),
      }
  };
  base::MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  TestingProfile profile;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    GURL url(test_data[i].test_url);
    WillHandleBrowserAboutURL(&url, &profile);
    EXPECT_EQ(test_data[i].result_url, url);
  }
}
