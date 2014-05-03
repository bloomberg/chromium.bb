// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_sign_in_container.h"

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_view_delegate.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace {

class AutofillSignInContainerTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillSignInContainerTest() : dialog_(&delegate_), test_window_(nil) {}
  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    // Inherting from ChromeRenderViewHostTestHarness means we can't inherit
    // from from CocoaTest, so do a bootstrap and create test window.
    CocoaTest::BootstrapCocoa();

    container_.reset(
        [[AutofillSignInContainer alloc] initWithDialog:&dialog_]);
    EXPECT_CALL(delegate_, profile())
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
  base::scoped_nsobject<AutofillSignInContainer> container_;
  testing::NiceMock<autofill::MockAutofillDialogViewDelegate> delegate_;
  autofill::AutofillDialogCocoa dialog_;
  CocoaTestHelperWindow* test_window_;
};

}  // namespace

TEST_VIEW(AutofillSignInContainerTest, [container_ view])

TEST_F(AutofillSignInContainerTest, Subviews) {
  // isKindOfClass would be the better choice, but
  // WebContentsViewCocoaClass is defined in content, and not public.
  bool hasWebView =[[container_ view] isEqual:
      [container_ webContents]->GetNativeView()];

  EXPECT_TRUE(hasWebView);
}

