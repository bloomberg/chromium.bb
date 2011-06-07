// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/common/about_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef TestingBrowserProcessTest BrowserAboutHandlerTest;

TEST_F(BrowserAboutHandlerTest, WillHandleBrowserAboutURL) {
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
        GURL(std::string(chrome::kAboutCacheURL) + "/mercury"),
        GURL(std::string(chrome::kNetworkViewCacheURL) + "mercury"),
        false,
        true
      },
      {
        GURL(std::string(chrome::kAboutNetInternalsURL) + "/venus"),
        GURL(std::string(chrome::kNetworkViewInternalsURL) + "venus"),
        false,
        true
      },
      {
        GURL(std::string(chrome::kAboutGpuURL) + "/jupiter"),
        GURL(std::string(chrome::kGpuInternalsURL) + "jupiter"),
        false,
        true
      },
      {
        GURL(std::string(chrome::kAboutAppCacheInternalsURL) + "/earth"),
        GURL(std::string(chrome::kAppCacheViewInternalsURL) + "earth"),
        false,
        true
      },
      {
        GURL(chrome::kAboutPluginsURL),
        GURL(chrome::kChromeUIPluginsURL),
        false,
        true
      },
      {
        GURL(chrome::kAboutCrashURL),
        GURL(chrome::kAboutCrashURL),
        true,
        false
      },
      {
        GURL(chrome::kAboutKillURL),
        GURL(chrome::kAboutKillURL),
        true,
        false
      },
      {
        GURL(chrome::kAboutHangURL),
        GURL(chrome::kAboutHangURL),
        true,
        false
      },
      {
        GURL(chrome::kAboutShorthangURL),
        GURL(chrome::kAboutShorthangURL),
        true,
        false
      },
      {
        GURL("about:memory"),
        GURL("chrome://about/memory-redirect"),
        false,
        true
      },
      {
        GURL("about:mars"),
        GURL("chrome://about/mars"),
        false,
        true
      },
  };
  MessageLoopForUI message_loop;
  BrowserThread ui_thread(BrowserThread::UI, &message_loop);
  TestingProfile profile;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    GURL url(test_data[i].test_url);
    EXPECT_EQ(test_data[i].about_handled,
              chrome_about_handler::WillHandle(url));
    EXPECT_EQ(test_data[i].browser_handled,
              WillHandleBrowserAboutURL(&url, &profile));
    EXPECT_EQ(test_data[i].result_url, url);
  }

  // Crash the browser process for about:inducebrowsercrashforrealz.
  GURL url(chrome::kAboutBrowserCrash);
  EXPECT_DEATH(WillHandleBrowserAboutURL(&url, NULL), "");
}
