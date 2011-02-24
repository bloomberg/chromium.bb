// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_view.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/test_tab_contents.h"

@interface SadTabView (ExposedForTesting)
// Implementation is below.
- (NSButton*)linkButton;
@end

@implementation SadTabView (ExposedForTesting)
- (NSButton*)linkButton {
  return linkButton_;
}
@end

namespace {

class SadTabControllerTest : public RenderViewHostTestHarness {
 public:
  SadTabControllerTest() : test_window_(nil) {
    link_clicked_ = false;
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    // Inherting from RenderViewHostTestHarness means we can't inherit from
    // from CocoaTest, so do a bootstrap and create test window.
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
    RenderViewHostTestHarness::TearDown();
  }

  // Creates the controller and adds its view to contents, caller has ownership.
  SadTabController* CreateController() {
    NSView* contentView = [test_window_ contentView];
    SadTabController* controller =
        [[SadTabController alloc] initWithTabContents:contents()
                                            superview:contentView];
    EXPECT_TRUE(controller);
    NSView* view = [controller view];
    EXPECT_TRUE(view);

    return controller;
  }

  NSButton* GetLinkButton(SadTabController* controller) {
    SadTabView* view = static_cast<SadTabView*>([controller view]);
    return ([view linkButton]);
  }

  static bool link_clicked_;
  CocoaTestHelperWindow* test_window_;
};

// static
bool SadTabControllerTest::link_clicked_;

TEST_F(SadTabControllerTest, WithTabContents) {
  scoped_nsobject<SadTabController> controller(CreateController());
  EXPECT_TRUE(controller);
  NSButton* link = GetLinkButton(controller);
  EXPECT_TRUE(link);
}

TEST_F(SadTabControllerTest, WithoutTabContents) {
  contents_.reset();
  scoped_nsobject<SadTabController> controller(CreateController());
  EXPECT_TRUE(controller);
  NSButton* link = GetLinkButton(controller);
  EXPECT_FALSE(link);
}

TEST_F(SadTabControllerTest, ClickOnLink) {
  scoped_nsobject<SadTabController> controller(CreateController());
  NSButton* link = GetLinkButton(controller);
  EXPECT_TRUE(link);
  EXPECT_FALSE(link_clicked_);
  [link performClick:link];
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
