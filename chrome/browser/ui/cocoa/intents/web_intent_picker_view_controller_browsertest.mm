// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_message_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_cocoa2.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_view_controller.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate_mock.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#import "testing/gtest_mac.h"

namespace {

NSString* const kTitle = @"A quick brown fox jumps over the lazy dog.";

}  // namespace

class WebIntentPickerViewControllerTest : public InProcessBrowserTest {
 public:
  WebIntentPickerViewControllerTest() : picker_(NULL) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    TabContents* tab = browser()->tab_strip_model()->GetTabContentsAt(0);
    picker_ =
        new WebIntentPickerCocoa2(tab->web_contents(), &delegate_, &model_);
    controller_ = picker_->view_controller();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    EXPECT_CALL(delegate_, OnClosing());
    NSWindow* sheet = [picker_->window_controller() window];
    ConstrainedWindowSheetController* sheetController =
        [ConstrainedWindowSheetController controllerForSheet:sheet];
    picker_->Close();
    [sheetController endAnimationForSheet:sheet];
    chrome::testing::NSRunLoopRunAllPending();
  }

 protected:
  WebIntentPickerDelegateMock delegate_;
  WebIntentPickerModel model_;
  WebIntentPickerCocoa2* picker_;  // weak
  WebIntentPickerViewController* controller_;  // weak
};

// Test that the view is created and embedded in the constrained window.
IN_PROC_BROWSER_TEST_F(WebIntentPickerViewControllerTest, View) {
  EXPECT_TRUE([controller_ view]);
  EXPECT_TRUE([[controller_ view] superview]);
  EXPECT_NSEQ([picker_->window_controller() window],
              [[controller_ view] window]);
}

// Test the close button.
IN_PROC_BROWSER_TEST_F(WebIntentPickerViewControllerTest, CloseButton) {
  EXPECT_NSEQ([controller_ view],
              [[controller_ closeButton] superview]);
  EXPECT_NSEQ(kKeyEquivalentEscape, [[controller_ closeButton] keyEquivalent]);
  EXPECT_CALL(delegate_, OnUserCancelledPickerDialog());
  [[controller_ closeButton] performClick:nil];
}

// Test the "no matching services" state.
IN_PROC_BROWSER_TEST_F(WebIntentPickerViewControllerTest, NoServices) {
  model_.SetWaitingForSuggestions(false);
  EXPECT_EQ(PICKER_STATE_NO_SERVICE, [controller_ state]);
  WebIntentMessageViewController* message_controller =
      [controller_ messageViewController];
  EXPECT_NSEQ([controller_ view], [[message_controller view] superview]);
}
