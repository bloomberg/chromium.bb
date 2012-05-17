// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/web_intent_sheet_controller.h"

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

class WebIntentPickerSheetControllerTest : public CocoaTest {
 public:

  virtual void SetUp() {
    CocoaTest::SetUp();
    controller_ = [[WebIntentPickerSheetController alloc] initWithPicker:nil];
    window_ = [controller_ window];
  }

  virtual void TearDown() {
    // Since we're not actually running the sheet, we must manually release.
    [controller_ release];

    CocoaTest::TearDown();
  }

  // Checks the controller's window for the requisite subviews and icons.
  void CheckWindow(size_t row_count) {
    NSArray* flip_views = [[window_ contentView] subviews];

    // Check for proper firstResponder.
    ASSERT_EQ(controller_, [window_ firstResponder]);

    // Expect 1 subview - the flip view.
    ASSERT_EQ(1U, [flip_views count]);

    NSArray* views = [[flip_views objectAtIndex:0] subviews];

    // 3 + |row_count| subviews - header text, close button,
    // |row_count| buttons, and a CWS link.
    ASSERT_EQ(3U + row_count, [views count]);

    ASSERT_TRUE([[views objectAtIndex:0] isKindOfClass:[NSTextField class]]);
    ASSERT_TRUE([[views objectAtIndex:1] isKindOfClass:
        [HoverCloseButton class]]);
    for(NSUInteger i = 0; i < row_count; ++i) {
      ASSERT_TRUE([[views objectAtIndex:2 + i] isKindOfClass:
          [NSButton class]]);
    }

    // Verify the close button
    NSButton* close_button = static_cast<NSButton*>([views objectAtIndex:1]);
    CheckButton(close_button, @selector(cancelOperation:));

    // Verify the Chrome Web Store button.
    NSButton* button = static_cast<NSButton*>([views lastObject]);
    ASSERT_TRUE([button isKindOfClass:[NSButton class]]);
    EXPECT_TRUE([[button cell] isKindOfClass:[HyperlinkButtonCell class]]);
    CheckButton(button, @selector(showChromeWebStore:));

    // Verify buttons pointing to services.
    for(NSUInteger i = 0; i < row_count; ++i) {
      NSButton* button = [views objectAtIndex:2 + i];
      CheckServiceButton(button, i);
    }
  }

  // Checks that a service button is hooked up correctly.
  void CheckServiceButton(NSButton* button, NSUInteger service_index) {
    CheckButton(button, @selector(invokeService:));
    EXPECT_EQ(NSInteger(service_index), [button tag]);
  }

  // Checks that a button is hooked up correctly.
  void CheckButton(id button, SEL action) {
    EXPECT_TRUE([button isKindOfClass:[NSButton class]] ||
      [button isKindOfClass:[NSButtonCell class]]);
    EXPECT_EQ(action, [button action]);
    EXPECT_EQ(controller_, [button target]);
    EXPECT_TRUE([button stringValue]);
  }

  // Controller under test.
  WebIntentPickerSheetController* controller_;

  NSWindow* window_;  // Weak, owned by |controller_|.
};

TEST_F(WebIntentPickerSheetControllerTest, NoRows) {
  CheckWindow(/*row_count=*/0);
}

TEST_F(WebIntentPickerSheetControllerTest, PopulatedRows) {
  WebIntentPickerModel model;
  model.AddInstalledService(string16(), GURL(),
      WebIntentPickerModel::DISPOSITION_WINDOW);
  model.AddInstalledService(string16(), GURL(),
      WebIntentPickerModel::DISPOSITION_WINDOW);

  [controller_ performLayoutWithModel:&model];

  CheckWindow(/*row_count=*/2);
}

TEST_F(WebIntentPickerSheetControllerTest, SuggestionView) {
  WebIntentPickerModel model;

  model.AddSuggestedExtension(string16(), string16(), 2.5);
  [controller_ performLayoutWithModel:&model];

  // Get subviews.
  NSArray* flip_views = [[window_ contentView] subviews];
  NSArray* main_views = [[flip_views objectAtIndex:0] subviews];

  // 3rd object should be the suggestion view.
  ASSERT_TRUE([main_views count] > 2);
  ASSERT_TRUE([[main_views objectAtIndex:2] isKindOfClass:[NSView class]]);
  NSView* suggest_view = [main_views objectAtIndex:2];

  // There are two subviews - label & suggested items.
  ASSERT_EQ(2U, [[suggest_view subviews] count]);
  ASSERT_TRUE([[[suggest_view subviews] objectAtIndex:1]
      isKindOfClass:[NSTextField class]]);
  ASSERT_TRUE([[[suggest_view subviews] objectAtIndex:0]
      isKindOfClass:[NSView class]]);
  NSView* item_view = [[suggest_view subviews] objectAtIndex:0];

  // 5 subobjects - Icon, title, star rating, add button, and throbber.
  ASSERT_EQ(5U, [[item_view subviews] count]);

  // Verify title button is hooked up properly
  ASSERT_TRUE([[[item_view subviews] objectAtIndex:1]
      isKindOfClass:[NSButton class]]);
  NSButton* title_button = [[item_view subviews] objectAtIndex:1];
  CheckButton(title_button, @selector(openExtensionLink:));

  // Verify "Add to Chromium" button is hooked up properly
  ASSERT_TRUE([[[item_view subviews] objectAtIndex:3]
      isKindOfClass:[NSButton class]]);
  NSButton* add_button = [[item_view subviews] objectAtIndex:3];
  CheckButton(add_button, @selector(installExtension:));

  // Verify we have a throbber.
  ASSERT_TRUE([[[item_view subviews] objectAtIndex:4]
      isKindOfClass:[NSProgressIndicator class]]);
}
