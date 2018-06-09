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

- (void)requestSuggestionsForText:(NSString*)text {
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
    feature_list.InitAndEnableFeature(features::kSuggestedTextTouchBar);
  }

  void SetUpOnMainThread() override {
    web_textfield_controller_.reset(
        [[MockWebTextfieldTouchBarController alloc] init]);
    [web_textfield_controller_ resetNumInvalidations];
  }

  MockSuggestedTextTouchBarController* focused_touch_bar_controller() const {
    ui_test_utils::NavigateToURL(
        browser(),
        GURL("data:text/html;charset=utf-8,<input type=\"text\" autofocus>"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return [[MockSuggestedTextTouchBarController alloc]
        initWithWebContents:web_contents
                 controller:web_textfield_controller_];
  }

  MockSuggestedTextTouchBarController* unfocused_touch_bar_controller() const {
    ui_test_utils::NavigateToURL(
        browser(), GURL("data:text/html;charset=utf-8,<input type=\"text\">"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return [[MockSuggestedTextTouchBarController alloc]
        initWithWebContents:web_contents
                 controller:web_textfield_controller_];
  }

  MockWebTextfieldTouchBarController* web_textfield_controller() const {
    return web_textfield_controller_;
  }

 private:
  base::scoped_nsobject<MockWebTextfieldTouchBarController>
      web_textfield_controller_;
  base::test::ScopedFeatureList feature_list;
};

// Tests that text is saved properly, regardless of whether a textfield is
// focused.
IN_PROC_BROWSER_TEST_F(SuggestedTextTouchBarControllerBrowserTest,
                       SetTextTest) {
  SuggestedTextTouchBarController* touch_bar_controller;

  touch_bar_controller = unfocused_touch_bar_controller();
  ASSERT_FALSE([touch_bar_controller webContents]->IsFocusedElementEditable());
  [touch_bar_controller setText:kEmptyText];
  EXPECT_EQ(kEmptyText, [touch_bar_controller text]);

  touch_bar_controller = focused_touch_bar_controller();
  ASSERT_TRUE([touch_bar_controller webContents]->IsFocusedElementEditable());
  [touch_bar_controller setText:kText];
  EXPECT_EQ(kText, [touch_bar_controller text]);
}

// Tests to check if the touch bar shows up properly.
IN_PROC_BROWSER_TEST_F(SuggestedTextTouchBarControllerBrowserTest,
                       TouchBarTest) {
  if (@available(macOS 10.12.2, *)) {
    SuggestedTextTouchBarController* touch_bar_controller;

    // Touch bar shouldn't appear if not in textfield.
    touch_bar_controller = unfocused_touch_bar_controller();
    ASSERT_FALSE(
        [touch_bar_controller webContents]->IsFocusedElementEditable());
    EXPECT_FALSE([touch_bar_controller makeTouchBar]);

    // Touch bar shouldn't appear if there is no text to make suggestions.
    touch_bar_controller = focused_touch_bar_controller();
    [touch_bar_controller setText:kEmptyText];
    ASSERT_TRUE([touch_bar_controller webContents]->IsFocusedElementEditable());
    ASSERT_EQ(kEmptyText, [touch_bar_controller text]);
    EXPECT_FALSE([touch_bar_controller makeTouchBar]);

    // Touch bar should appear if in textfield and text exists for suggestions.
    [touch_bar_controller setText:kText];
    ASSERT_TRUE([touch_bar_controller webContents]->IsFocusedElementEditable());
    ASSERT_EQ(kText, [touch_bar_controller text]);
    NSTouchBar* touch_bar = [touch_bar_controller makeTouchBar];
    EXPECT_TRUE(touch_bar);
    EXPECT_TRUE([[touch_bar customizationIdentifier]
        isEqual:ui::GetTouchBarId(kSuggestedTextTouchBarId)]);
  }
}

// Tests that a change in text selection is handled properly.
IN_PROC_BROWSER_TEST_F(SuggestedTextTouchBarControllerBrowserTest,
                       TextSelectionChangedTest) {
  MockSuggestedTextTouchBarController* touch_bar_controller;

  // If not in a textfield,
  //  1. New text selection should not be saved.
  //  2. Touch bar should be invalidated.
  touch_bar_controller = unfocused_touch_bar_controller();
  [touch_bar_controller setText:kEmptyText];
  ASSERT_FALSE([touch_bar_controller webContents]->IsFocusedElementEditable());
  ASSERT_EQ(kEmptyText, [touch_bar_controller text]);
  [touch_bar_controller webContentsTextSelectionChanged:kText];
  EXPECT_EQ(kEmptyText, [touch_bar_controller text]);
  EXPECT_EQ(1, [web_textfield_controller() numInvalidations]);

  // If in a textfield,
  //   1. New text selection should be saved.
  //   2. Suggestions should be generated based on text selection.
  //   3. Touch bar should be invalidated.
  touch_bar_controller = focused_touch_bar_controller();
  [touch_bar_controller setText:kEmptyText];
  ASSERT_TRUE([touch_bar_controller webContents]->IsFocusedElementEditable());
  ASSERT_EQ(kEmptyText, [touch_bar_controller text]);
  [touch_bar_controller webContentsTextSelectionChanged:kText];
  EXPECT_EQ(kText, [touch_bar_controller text]);
  EXPECT_EQ(kText, [touch_bar_controller firstSuggestion]);
  EXPECT_EQ(2, [web_textfield_controller() numInvalidations]);
}

}  // namespace
