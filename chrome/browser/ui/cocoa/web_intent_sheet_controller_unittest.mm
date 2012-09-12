// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/web_intent_sheet_controller.h"

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

@class HeaderView;

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

  void CheckHeader(NSView* header_view) {
    ASSERT_TRUE([header_view isKindOfClass:[NSView class]]);
    NSArray* views = [header_view subviews];

    ASSERT_EQ(3U, [views count]);
    ASSERT_TRUE([[views objectAtIndex:0] isKindOfClass:[NSTextField class]]);
    ASSERT_TRUE([[views objectAtIndex:1] isKindOfClass:[NSTextField class]]);
    ASSERT_TRUE([[views objectAtIndex:2] isKindOfClass:[NSBox class]]);
  }

  // Checks the controller's window for the requisite subviews and icons.
  void CheckWindow(size_t row_count) {
    NSArray* flip_views = [[window_ contentView] subviews];

    // Expect 1 subview - the flip view.
    ASSERT_EQ(1U, [flip_views count]);

    NSArray* views = [[flip_views objectAtIndex:0] subviews];

    // 4 subviews - header view, intents list, CWS link, close button.
    // intents list is not added if there are no rows.
    NSUInteger view_offset = row_count ? 1U : 0U;
    ASSERT_EQ(3U + view_offset, [views count]);

    ASSERT_TRUE([[views objectAtIndex:0] isKindOfClass:[NSView class]]);
    CheckHeader([views objectAtIndex:0]);
    if (view_offset) {
      ASSERT_TRUE([[views objectAtIndex:1] isKindOfClass:[NSView class]]);
    }
    ASSERT_TRUE([[views objectAtIndex:1 + view_offset] isKindOfClass:
                 [NSButton class]]);
    ASSERT_TRUE([[views objectAtIndex:2 + view_offset] isKindOfClass:
        [HoverCloseButton class]]);

    // Verify the close button
    NSButton* close_button = static_cast<NSButton*>([views lastObject]);
    CheckButton(close_button, @selector(cancelOperation:));

    // Verify the Chrome Web Store button.
    NSButton* button = static_cast<NSButton*>(
        [views objectAtIndex:1 + view_offset]);
    EXPECT_TRUE([[button cell] isKindOfClass:[HyperlinkButtonCell class]]);
    CheckButton(button, @selector(showChromeWebStore:));
  }

  void CheckSuggestionView(NSView* item_view, NSUInteger suggestion_index) {
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
    EXPECT_EQ(NSInteger(suggestion_index), [add_button tag]);

    // Verify we have a throbber.
    ASSERT_TRUE([[[item_view subviews] objectAtIndex:4]
        isKindOfClass:[NSProgressIndicator class]]);
  }

  void CheckServiceView(NSView* item_view, NSUInteger service_index) {
    // 3 subobjects - Icon, title, select button.
    ASSERT_EQ(3U, [[item_view subviews] count]);

    // Verify title is a text field.
    ASSERT_TRUE([[[item_view subviews] objectAtIndex:1]
        isKindOfClass:[NSTextField class]]);

    // Verify "Select" button is hooked up properly.
    ASSERT_TRUE([[[item_view subviews] objectAtIndex:2]
        isKindOfClass:[NSButton class]]);
    NSButton* select_button = [[item_view subviews] objectAtIndex:2];
    CheckButton(select_button, @selector(invokeService:));
    EXPECT_EQ(NSInteger(service_index), [select_button tag]);
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

TEST_F(WebIntentPickerSheetControllerTest, IntentRows) {
  WebIntentPickerModel model;
  model.AddInstalledService(string16(), GURL("http://example.org/intent.html"),
      webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW);
  model.AddInstalledService(string16(), GURL("http://example.com/intent.html"),
      webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW);
  model.SetWaitingForSuggestions(false);
  [controller_ performLayoutWithModel:&model];

  CheckWindow(/*row_count=*/2);

  NSArray* flip_views = [[window_ contentView] subviews];
  NSArray* main_views = [[flip_views objectAtIndex:0] subviews];

  // 2nd object should be the suggestion view, 3rd one is close button.
  ASSERT_TRUE([main_views count] > 2);
  ASSERT_TRUE([[main_views objectAtIndex:1] isKindOfClass:[NSView class]]);
  NSView* intent_view = [main_views objectAtIndex:1];

  // 2 subviews - the two installed services item. Tags are assigned reverse.
  ASSERT_EQ(2U, [[intent_view subviews] count]);
  NSView* item_view = [[intent_view subviews] objectAtIndex:0];
  CheckServiceView(item_view, 1);
  item_view = [[intent_view subviews] objectAtIndex:1];
  CheckServiceView(item_view, 0);
}

TEST_F(WebIntentPickerSheetControllerTest, SuggestionRow) {
  WebIntentPickerModel model;
  std::vector<WebIntentPickerModel::SuggestedExtension> suggestions;
  suggestions.push_back(WebIntentPickerModel::SuggestedExtension(
      string16(), string16(), 2.5));
  model.AddSuggestedExtensions(suggestions);
  model.SetWaitingForSuggestions(false);

  [controller_ performLayoutWithModel:&model];

  CheckWindow(/*row_count=*/1);

  // Get subviews.
  NSArray* flip_views = [[window_ contentView] subviews];
  NSArray* main_views = [[flip_views objectAtIndex:0] subviews];

  // 2nd object should be the suggestion view, 3rd one is close button.
  ASSERT_TRUE([main_views count] > 2);
  ASSERT_TRUE([[main_views objectAtIndex:1] isKindOfClass:[NSView class]]);
  NSView* intent_view = [main_views objectAtIndex:1];

  // One subview - the suggested item.
  ASSERT_EQ(1U, [[intent_view subviews] count]);
  ASSERT_TRUE([[[intent_view subviews] objectAtIndex:0]
      isKindOfClass:[NSView class]]);
  NSView* item_view = [[intent_view subviews] objectAtIndex:0];
  CheckSuggestionView(item_view, 0);
}

TEST_F(WebIntentPickerSheetControllerTest, MixedIntentView) {
  WebIntentPickerModel model;
  std::vector<WebIntentPickerModel::SuggestedExtension> suggestions;
  suggestions.push_back(WebIntentPickerModel::SuggestedExtension(
      string16(), string16(), 2.5));
  model.AddSuggestedExtensions(suggestions);
  model.AddInstalledService(string16(), GURL("http://example.org/intent.html"),
      webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW);
  model.AddInstalledService(string16(), GURL("http://example.com/intent.html"),
      webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW);
  model.SetWaitingForSuggestions(false);

  [controller_ performLayoutWithModel:&model];

  CheckWindow(/*row_count=*/3);

  NSArray* flip_views = [[window_ contentView] subviews];
  NSArray* main_views = [[flip_views objectAtIndex:0] subviews];

  // 2nd object should be the suggestion view, 3rd one is close button.
  ASSERT_TRUE([main_views count] > 2);
  ASSERT_TRUE([[main_views objectAtIndex:1] isKindOfClass:[NSView class]]);
  NSView* intent_view = [main_views objectAtIndex:1];

  // 3 subviews - 2 installed services, 1 suggestion.
  ASSERT_EQ(3U, [[intent_view subviews] count]);
  NSView* item_view = [[intent_view subviews] objectAtIndex:0];
  CheckSuggestionView(item_view, 0);
  item_view = [[intent_view subviews] objectAtIndex:1];
  CheckServiceView(item_view, 1);
  item_view = [[intent_view subviews] objectAtIndex:2];
  CheckServiceView(item_view, 0);
}

TEST_F(WebIntentPickerSheetControllerTest, EmptyView) {
  WebIntentPickerModel model;
  model.SetWaitingForSuggestions(false);
  [controller_ performLayoutWithModel:&model];

  ASSERT_TRUE(window_);

  // Get subviews.
  NSArray* flip_views = [[window_ contentView] subviews];
  ASSERT_TRUE(flip_views);

  NSArray* main_views = [[flip_views objectAtIndex:0] subviews];
  ASSERT_TRUE(main_views);

  // Should have two subviews - the empty picker dialog and the close button.
  ASSERT_EQ(2U, [main_views count]);

  // Extract empty picker dialog.
  ASSERT_TRUE([[main_views objectAtIndex:0] isKindOfClass:[NSView class]]);
  NSView* empty_dialog = [main_views objectAtIndex:0];

  // Empty picker dialog has two elements, title and body.
  ASSERT_EQ(2U, [[empty_dialog subviews] count]);

  // Both title and body are NSTextFields.
  ASSERT_TRUE([[[empty_dialog subviews] objectAtIndex:0]
      isKindOfClass:[NSTextField class]]);
  ASSERT_TRUE([[[empty_dialog subviews] objectAtIndex:1]
      isKindOfClass:[NSTextField class]]);
}
