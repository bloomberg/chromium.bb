// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/touchbar/suggested_text_touch_bar_controller.h"
#include "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/cocoa/touch_bar_util.h"

@interface MockWebTextfieldTouchBarController : WebTextfieldTouchBarController {
  // Counter for the number of times invalidateTouchBar is called.
  int numInvalidations_;
}
- (int)numInvalidations;

- (void)resetNumInvalidations;

@end

@implementation MockWebTextfieldTouchBarController

- (void)invalidateTouchBar {
  numInvalidations_++;
}

- (int)numInvalidations {
  return numInvalidations_;
}

- (void)resetNumInvalidations {
  numInvalidations_ = 0;
}

@end

@interface MockSuggestedTextTouchBarController : SuggestedTextTouchBarController
- (NSString*)firstSuggestion;
@end

@implementation MockSuggestedTextTouchBarController

- (void)requestSuggestionsForText:(NSString*)text inRange:(NSRange)range {
  [self setSuggestions:@[ text ]];
  [[self controller] invalidateTouchBar];
}

- (NSString*)firstSuggestion {
  return [self suggestions][0];
}

@end

namespace {

NSString* const kSuggestedTextTouchBarId = @"suggested-text";
NSString* const kText = @"text";
NSString* const kEmptyText = @"";

class SuggestedTextTouchBarControllerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    InProcessBrowserTest::SetUp();
    feature_list_.InitAndEnableFeature(features::kSuggestedTextTouchBar);
  }

  void SetUpOnMainThread() override {
    web_textfield_controller_.reset(
        [[MockWebTextfieldTouchBarController alloc] init]);
    [web_textfield_controller_ resetNumInvalidations];
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    touch_bar_controller_.reset([[MockSuggestedTextTouchBarController alloc]
        initWithWebContents:web_contents
                 controller:web_textfield_controller_]);
  }

  void FocusTextfield() {
    ui_test_utils::NavigateToURL(
        browser(),
        GURL("data:text/html;charset=utf-8,<input type=\"text\" autofocus>"));
  }

  void UnfocusTextfield() {
    ui_test_utils::NavigateToURL(
        browser(), GURL("data:text/html;charset=utf-8,<input type=\"text\">"));
  }

  MockWebTextfieldTouchBarController* web_textfield_controller() const {
    return web_textfield_controller_;
  }

  MockSuggestedTextTouchBarController* touch_bar_controller() const {
    return touch_bar_controller_;
  }

 private:
  base::scoped_nsobject<MockWebTextfieldTouchBarController>
      web_textfield_controller_;
  base::scoped_nsobject<MockSuggestedTextTouchBarController>
      touch_bar_controller_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests to check if the touch bar shows up properly.
IN_PROC_BROWSER_TEST_F(SuggestedTextTouchBarControllerBrowserTest,
                       TouchBarTest) {
  if (@available(macOS 10.12.2, *)) {
    // Touch bar shouldn't appear if the focused element is not a textfield.
    UnfocusTextfield();
    EXPECT_FALSE([touch_bar_controller() makeTouchBar]);

    // Touch bar should appear if a textfield is focused.
    FocusTextfield();
    NSTouchBar* touch_bar = [touch_bar_controller() makeTouchBar];
    EXPECT_TRUE(touch_bar);
    EXPECT_TRUE([[touch_bar customizationIdentifier]
        isEqual:ui::GetTouchBarId(kSuggestedTextTouchBarId)]);
  }
}

// Tests that a change in text selection is handled properly.
IN_PROC_BROWSER_TEST_F(SuggestedTextTouchBarControllerBrowserTest,
                       TextSelectionChangedTest) {
  if (@available(macOS 10.12.2, *)) {
    const NSRange kRange = NSMakeRange(0, [kText length]);
    const NSRange kEmptyRange = NSMakeRange(0, 0);

    // If not in a textfield,
    //  1. Textfield text should not be saved.
    //  2. Selected range should not be saved.
    //  3. Touch bar should be invalidated.
    UnfocusTextfield();
    [touch_bar_controller() setText:kEmptyText];
    [touch_bar_controller() setSelectionRange:kEmptyRange];
    [web_textfield_controller() resetNumInvalidations];

    [touch_bar_controller()
        webContentsTextSelectionChanged:base::SysNSStringToUTF16(kText)
                                  range:kRange];
    EXPECT_TRUE([kEmptyText isEqualToString:[touch_bar_controller() text]]);
    EXPECT_EQ(gfx::Range(kEmptyRange),
              gfx::Range([touch_bar_controller() selectionRange]));
    EXPECT_EQ(1, [web_textfield_controller() numInvalidations]);

    // If in a textfield,
    //   1. Textfield text should be saved.
    //   2. Selected range should be saved.
    //   3. Suggestions should be generated based on text selection.
    //   4. Touch bar should be invalidated.
    FocusTextfield();
    [touch_bar_controller() setText:kEmptyText];
    [touch_bar_controller() setSelectionRange:kEmptyRange];
    [web_textfield_controller() resetNumInvalidations];

    [touch_bar_controller()
        webContentsTextSelectionChanged:base::SysNSStringToUTF16(kText)
                                  range:kRange];
    EXPECT_TRUE([kText isEqualToString:[touch_bar_controller() text]]);
    EXPECT_EQ(gfx::Range(kRange),
              gfx::Range([touch_bar_controller() selectionRange]));
    EXPECT_TRUE(
        [kText isEqualToString:[touch_bar_controller() firstSuggestion]]);
    EXPECT_EQ(1, [web_textfield_controller() numInvalidations]);
  }
}

}  // namespace