// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/translate/translate_bubble_controller.h"

#include "base/message_loop/message_loop.h"
#import "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/run_loop_testing.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/site_instance.h"

@implementation BrowserWindowController (ForTesting)

- (TranslateBubbleController*)translateBubbleController{
  return translateBubbleController_;
}

@end

class TranslateBubbleControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();
    site_instance_ = content::SiteInstance::Create(profile());
  }

  content::WebContents* AppendToTabStrip() {
    content::WebContents* web_contents = content::WebContents::Create(
        content::WebContents::CreateParams(profile(), site_instance_.get()));
    browser()->tab_strip_model()->AppendWebContents(
        web_contents, /*foreground=*/true);
    return web_contents;
   }

 private:
  scoped_refptr<content::SiteInstance> site_instance_;
};

TEST_F(TranslateBubbleControllerTest, ShowAndClose) {
  NSWindow* nativeWindow = browser()->window()->GetNativeWindow();
  BrowserWindowController* bwc =
      [BrowserWindowController browserWindowControllerForWindow:nativeWindow];
  content::WebContents* webContents = AppendToTabStrip();
  translate::TranslateStep step = translate::TRANSLATE_STEP_BEFORE_TRANSLATE;

  TranslateBubbleController* bubble = [bwc translateBubbleController];
  EXPECT_FALSE(bubble);

  [bwc showTranslateBubbleForWebContents:webContents
                                    step:step
                               errorType:translate::TranslateErrors::NONE];
  bubble = [bwc translateBubbleController];
  EXPECT_TRUE(bubble);
  InfoBubbleWindow* window = (InfoBubbleWindow*)[bubble window];
  [window setAllowedAnimations:info_bubble::kAnimateNone];

  [bubble close];
  chrome::testing::NSRunLoopRunAllPending();
  bubble = [bwc translateBubbleController];
  EXPECT_FALSE(bubble);
}
