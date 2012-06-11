// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/content_settings/collected_cookies_mac.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"

using content::BrowserThread;

namespace {

class CollectedCookiesWindowControllerTest
    : public TabContentsTestHarness {
 public:
  CollectedCookiesWindowControllerTest()
      : ui_thread_(BrowserThread::UI, MessageLoopForUI::current()) {
  }

 private:
  content::TestBrowserThread ui_thread_;
};

TEST_F(CollectedCookiesWindowControllerTest, Construction) {
  CollectedCookiesWindowController* controller =
      [[CollectedCookiesWindowController alloc]
          initWithTabContents:tab_contents()];

  [controller release];
}

}  // namespace

