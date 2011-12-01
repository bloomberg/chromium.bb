// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/common/about_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

typedef testing::Test BrowserAboutHandlerTest;

TEST_F(BrowserAboutHandlerTest, WillHandleBrowserAboutURL) {
  std::string chrome_prefix(chrome::kChromeUIScheme);
  chrome_prefix.append(chrome::kStandardSchemeSeparator);
  struct AboutURLTestData {
    GURL test_url;
    GURL result_url;
    bool about_handled;
    bool browser_handled;
  } test_data[] = {
      {
        GURL("http://google.com"),
        GURL("http://google.com"),
        false,
        false
      },
      {
        GURL(chrome::kAboutBlankURL),
        GURL(chrome::kAboutBlankURL),
        false,
        false
      },
      {
        GURL(chrome_prefix + chrome::kChromeUICrashHost),
        GURL(chrome_prefix + chrome::kChromeUICrashHost),
        true,
        false
      },
      {
        GURL(chrome_prefix + chrome::kChromeUIKillHost),
        GURL(chrome_prefix + chrome::kChromeUIKillHost),
        true,
        false
      },
      {
        GURL(chrome_prefix + chrome::kChromeUIHangHost),
        GURL(chrome_prefix + chrome::kChromeUIHangHost),
        true,
        false
      },
      {
        GURL(chrome_prefix + chrome::kChromeUIShorthangHost),
        GURL(chrome_prefix + chrome::kChromeUIShorthangHost),
        true,
        false
      },
      {
        GURL(chrome_prefix + chrome::kChromeUIMemoryHost),
        GURL(chrome_prefix + chrome::kChromeUIMemoryHost),
        false,
        false
      },
      {
        GURL(chrome_prefix + chrome::kChromeUIDefaultHost),
        GURL(chrome_prefix + chrome::kChromeUIVersionHost),
        false,
        false
      },
      {
        GURL(chrome_prefix + chrome::kChromeUIAboutHost),
        GURL(chrome_prefix + chrome::kChromeUIChromeURLsHost),
        false,
        false
      },
      {
        GURL(chrome_prefix + chrome::kChromeUICacheHost),
        GURL(chrome_prefix + chrome::kChromeUINetworkViewCacheHost),
        false,
        false
      },
      {
        GURL(chrome_prefix + chrome::kChromeUIGpuHost),
        GURL(chrome_prefix + chrome::kChromeUIGpuInternalsHost),
        false,
        false
      },
      {
        GURL(chrome_prefix + chrome::kChromeUISyncHost),
        GURL(chrome_prefix + chrome::kChromeUISyncInternalsHost),
        false,
        false
      },
      {
        GURL(chrome_prefix + "host/path?query#ref"),
        GURL(chrome_prefix + "host/path?query#ref"),
        false,
        false
      }
  };
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  TestingProfile profile;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    GURL url(test_data[i].test_url);
    EXPECT_EQ(test_data[i].about_handled,
              chrome_about_handler::WillHandle(url));
    EXPECT_EQ(test_data[i].browser_handled,
              WillHandleBrowserAboutURL(&url, &profile));
    EXPECT_EQ(test_data[i].result_url, url);
  }

  // Crash the browser process for chrome://inducebrowsercrashforrealz.
  GURL url(chrome_prefix + chrome::kChromeUIBrowserCrashHost);
  EXPECT_DEATH(HandleNonNavigationAboutURL(url), "");
}
