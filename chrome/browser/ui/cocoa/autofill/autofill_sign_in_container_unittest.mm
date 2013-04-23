// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_sign_in_container.h"

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_controller.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

@interface AutofillSignInContainer (ExposedForTesting)
- (content::WebContents*)webContents;
@end

@implementation AutofillSignInContainer (ExposedForTesting)
- (content::WebContents*)webContents { return webContents_.get(); }
@end

namespace {

class AutofillSignInContainerTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillSignInContainerTest() : test_window_(nil) {}
  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    // Inherting from ChromeRenderViewHostTestHarness means we can't inherit
    // from from CocoaTest, so do a bootstrap and create test window.
    CocoaTest::BootstrapCocoa();
    container_.reset(
        [[AutofillSignInContainer alloc] initWithController:&controller_]);
    EXPECT_CALL(controller_, profile())
        .WillOnce(testing::Return(this->profile()));
    [[test_window() contentView] addSubview:[container_ view]];
  }

  virtual void TearDown() {
    container_.reset();  // must reset at teardown - depends on test harness.
    [test_window_ close];
    test_window_ = nil;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  CocoaTestHelperWindow* test_window() {
    if (!test_window_) {
      test_window_ = [[CocoaTestHelperWindow alloc] init];
      if (base::debug::BeingDebugged()) {
        [test_window_ orderFront:nil];
      } else {
        [test_window_ orderBack:nil];
      }
    }
    return test_window_;
  }

 protected:
  scoped_nsobject<AutofillSignInContainer> container_;
  testing::NiceMock<autofill::MockAutofillDialogController> controller_;
  CocoaTestHelperWindow* test_window_;
};

}  // namespace

TEST_VIEW(AutofillSignInContainerTest, [container_ view])

TEST_F(AutofillSignInContainerTest, Subviews) {
  bool hasButton = false;
  bool hasWebView = false;

  for (NSView* view in [[container_ view] subviews]) {
    if ([view isKindOfClass:[NSButton class]]) {
      hasButton = true;
    } else {
      // isKindOfClass would be the better choice, but
      // WebContentsViewCocoaClass is defined in content, and not public.
      hasWebView = [view isEqual:
                       [container_ webContents]->GetView()->GetNativeView()];
    }
  }

  EXPECT_TRUE(hasButton);
  EXPECT_TRUE(hasWebView);
}