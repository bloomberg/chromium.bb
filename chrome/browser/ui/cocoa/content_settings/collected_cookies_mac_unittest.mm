// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/content_settings/collected_cookies_mac.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "chrome/test/base/testing_profile.h"

namespace {

class CollectedCookiesWindowControllerTest
    : public TabContentsWrapperTestHarness {
 public:
  CollectedCookiesWindowControllerTest()
      : ui_thread_(BrowserThread::UI, MessageLoopForUI::current()) {
  }

 private:
  BrowserThread ui_thread_;
};

TEST_F(CollectedCookiesWindowControllerTest, Construction) {
  CollectedCookiesWindowController* controller =
      [[CollectedCookiesWindowController alloc]
          initWithTabContentsWrapper:contents_wrapper()];

  [controller release];
}

}  // namespace

