// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"

@interface SadTabView (ExposedForTesting)
// Implementation is below.
- (HyperlinkTextView*)helpTextView;
@end

@implementation SadTabView (ExposedForTesting)
- (HyperlinkTextView*)helpTextView {
  return help_.get();
}
@end

namespace {

class SadTabControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  SadTabControllerTest() : test_window_(nil) {
    link_clicked_ = false;
  }

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    // Inherting from ChromeRenderViewHostTestHarness means we can't inherit
    // from from CocoaTest, so do a bootstrap and create test window.
    CocoaTest::BootstrapCocoa();
    test_window_ = [[CocoaTestHelperWindow alloc] init];
    if (base::debug::BeingDebugged()) {
      [test_window_ orderFront:nil];
    } else {
      [test_window_ orderBack:nil];
    }
  }

  virtual void TearDown() {
    [test_window_ close];
    test_window_ = nil;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Creates the controller and adds its view to contents, caller has ownership.
  SadTabController* CreateController() {
    SadTabController* controller =
        [[SadTabController alloc] initWithWebContents:web_contents()];
    EXPECT_TRUE(controller);
    NSView* view = [controller view];
    EXPECT_TRUE(view);
    NSView* contentView = [test_window_ contentView];
    [contentView addSubview:view];

    return controller;
  }

  HyperlinkTextView* GetHelpTextView(SadTabController* controller) {
    SadTabView* view = static_cast<SadTabView*>([controller view]);
    return ([view helpTextView]);
  }

  static bool link_clicked_;
  CocoaTestHelperWindow* test_window_;
};

// static
bool SadTabControllerTest::link_clicked_;

TEST_F(SadTabControllerTest, WithTabContents) {
  base::scoped_nsobject<SadTabController> controller(CreateController());
  EXPECT_TRUE(controller);
  HyperlinkTextView* help = GetHelpTextView(controller);
  EXPECT_TRUE(help);
}

TEST_F(SadTabControllerTest, WithoutTabContents) {
  DeleteContents();
  base::scoped_nsobject<SadTabController> controller(CreateController());
  EXPECT_TRUE(controller);
  HyperlinkTextView* help = GetHelpTextView(controller);
  EXPECT_FALSE(help);
}

TEST_F(SadTabControllerTest, ClickOnLink) {
  base::scoped_nsobject<SadTabController> controller(CreateController());
  HyperlinkTextView* help = GetHelpTextView(controller);
  EXPECT_TRUE(help);
  EXPECT_FALSE(link_clicked_);
  [help clickedOnLink:nil atIndex:0];
  EXPECT_TRUE(link_clicked_);
}

}  // namespace

@implementation NSApplication (SadTabControllerUnitTest)
// Add handler for the openLearnMoreAboutCrashLink: action to NSApp for testing
// purposes. Normally this would be sent up the responder tree correctly, but
// since tests run in the background, key window and main window are never set
// on NSApplication. Adding it to NSApplication directly removes the need for
// worrying about what the current window with focus is.
- (void)openLearnMoreAboutCrashLink:(id)sender {
  SadTabControllerTest::link_clicked_ = true;
}

@end
