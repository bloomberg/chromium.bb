// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/content_settings/collected_cookies_mac.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/test_browser_thread.h"

using content::BrowserThread;

namespace {

class CollectedCookiesWindowControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  CollectedCookiesWindowControllerTest()
      : ui_thread_(BrowserThread::UI, MessageLoopForUI::current()) {
  }

 private:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    TabSpecificContentSettings::CreateForWebContents(web_contents());
  }

  content::TestBrowserThread ui_thread_;
};

TEST_F(CollectedCookiesWindowControllerTest, Construction) {
  CollectedCookiesWindowController* controller =
      [[CollectedCookiesWindowController alloc]
          initWithWebContents:web_contents()];

  [controller release];
}

}  // namespace

