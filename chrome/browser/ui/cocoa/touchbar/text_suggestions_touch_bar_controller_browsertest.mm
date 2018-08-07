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
#import "chrome/browser/ui/cocoa/touchbar/text_suggestions_touch_bar_controller.h"
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

@interface MockTextSuggestionsTouchBarController
    : TextSuggestionsTouchBarController
- (NSString*)firstSuggestion;
@end

@implementation MockTextSuggestionsTouchBarController

- (void)requestSuggestions {
  [self setSuggestions:@[ [self text] ]];
  [[self controller] invalidateTouchBar];
}

- (NSString*)firstSuggestion {
  return [self suggestions][0];
}

@end

namespace {

class TextSuggestionsTouchBarControllerTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    InProcessBrowserTest::SetUp();
    feature_list_.InitAndEnableFeature(features::kTextSuggestionsTouchBar);
  }

  void SetUpOnMainThread() override {
    web_textfield_controller_.reset(
        [[MockWebTextfieldTouchBarController alloc] init]);
    [web_textfield_controller_ resetNumInvalidations];
    touch_bar_controller_.reset([[MockTextSuggestionsTouchBarController alloc]
        initWithWebContents:GetActiveWebContents()
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

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::scoped_nsobject<MockWebTextfieldTouchBarController>
      web_textfield_controller_;
  base::scoped_nsobject<MockTextSuggestionsTouchBarController>
      touch_bar_controller_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests to check if the touch bar shows up properly.
// DISABLED because it consistently fails "Mac10.12 Tests"
// https://crbug.com/871740
IN_PROC_BROWSER_TEST_F(TextSuggestionsTouchBarControllerTest,
                       DISABLED_MakeTouchBar) {
  if (@available(macOS 10.12.2, *)) {
    NSString* const kTextSuggestionsTouchBarId = @"text-suggestions";

    // Touch bar shouldn't appear if the focused element is not a textfield.
    UnfocusTextfield();
    EXPECT_FALSE([touch_bar_controller_ makeTouchBar]);

    // Touch bar should appear if a textfield is focused.
    FocusTextfield();
    NSTouchBar* touch_bar = [touch_bar_controller_ makeTouchBar];
    EXPECT_TRUE(touch_bar);
    EXPECT_TRUE([[touch_bar customizationIdentifier]
        isEqual:ui::GetTouchBarId(kTextSuggestionsTouchBarId)]);
  }
}

// Tests that a change in text selection is handled properly.
// DISABLED because it consistently fails "Mac10.12 Tests"
// https://crbug.com/871740
IN_PROC_BROWSER_TEST_F(TextSuggestionsTouchBarControllerTest,
                       DISABLED_UpdateTextSelection) {
  NSString* const kText = @"text";
  NSString* const kEmptyText = @"";
  const gfx::Range kRange = gfx::Range(0, 4);
  const gfx::Range kEmptyRange = gfx::Range();

  // If not in a textfield,
  //  1. Textfield text should not be saved.
  //  2. Selected range should not be saved.
  //  3. Touch bar should be invalidated (if on MacOS 10.12.2 or later).
  UnfocusTextfield();
  [touch_bar_controller_ setText:kEmptyText];
  [touch_bar_controller_ setSelectionRange:kEmptyRange];
  [web_textfield_controller_ resetNumInvalidations];

  [touch_bar_controller_ updateTextSelection:base::SysNSStringToUTF16(kText)
                                       range:kRange];
  EXPECT_STREQ(kEmptyText.UTF8String, [touch_bar_controller_ text].UTF8String);
  EXPECT_EQ(kEmptyRange, [touch_bar_controller_ selectionRange]);
  if (@available(macOS 10.12.2, *))
    EXPECT_EQ(1, [web_textfield_controller_ numInvalidations]);
  else
    EXPECT_EQ(0, [web_textfield_controller_ numInvalidations]);

  // If in a textfield and on MacOS 10.12.2 or later,
  //   1. Textfield text should be saved.
  //   2. Selected range should be saved.
  //   3. Suggestions should be generated based on text selection.
  //   4. Touch bar should be invalidated.
  FocusTextfield();
  [touch_bar_controller_ setText:kEmptyText];
  [touch_bar_controller_ setSelectionRange:kEmptyRange];
  [web_textfield_controller_ resetNumInvalidations];

  [touch_bar_controller_ updateTextSelection:base::SysNSStringToUTF16(kText)
                                       range:kRange];
  if (@available(macOS 10.12.2, *)) {
    EXPECT_STREQ(kText.UTF8String, [touch_bar_controller_ text].UTF8String);
    EXPECT_EQ(kRange, [touch_bar_controller_ selectionRange]);
    EXPECT_STREQ(kText.UTF8String,
                 [touch_bar_controller_ firstSuggestion].UTF8String);
    EXPECT_EQ(1, [web_textfield_controller_ numInvalidations]);
  } else {
    EXPECT_STREQ(kEmptyText.UTF8String,
                 [touch_bar_controller_ text].UTF8String);
    EXPECT_EQ(kEmptyRange, [touch_bar_controller_ selectionRange]);
    EXPECT_EQ(0, [web_textfield_controller_ numInvalidations]);
  }
}

// Tests that a change in WebContents is handled properly.
// DISABLED because it consistently fails "Mac10.12 Tests"
// https://crbug.com/871740
IN_PROC_BROWSER_TEST_F(TextSuggestionsTouchBarControllerTest,
                       DISABLED_SetWebContents) {
  NSString* const kText = @"text";
  const gfx::Range kRange = gfx::Range(1, 1);

  // A null-pointer should not break the controller.
  UnfocusTextfield();
  [touch_bar_controller_ setWebContents:nullptr];
  EXPECT_FALSE([touch_bar_controller_ webContents]);

  FocusTextfield();
  [touch_bar_controller_ setText:kText];
  [touch_bar_controller_ setSelectionRange:kRange];

  // The text selection should change on MacOS 10.12.2 and later if the
  // WebContents pointer is not null.
  [touch_bar_controller_ setWebContents:GetActiveWebContents()];
  EXPECT_EQ(GetActiveWebContents(), [touch_bar_controller_ webContents]);
  if (@available(macOS 10.12.2, *)) {
    EXPECT_STRNE(kText.UTF8String, [touch_bar_controller_ text].UTF8String);
    EXPECT_NE(kRange, [touch_bar_controller_ selectionRange]);
  } else {
    EXPECT_STREQ(kText.UTF8String, [touch_bar_controller_ text].UTF8String);
    EXPECT_EQ(kRange, [touch_bar_controller_ selectionRange]);
  }
}

}  // namespace
