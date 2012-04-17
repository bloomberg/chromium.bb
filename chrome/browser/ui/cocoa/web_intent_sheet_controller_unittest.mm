// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/message_loop.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/web_intent_sheet_controller.h"
#include "chrome/browser/ui/cocoa/web_intent_picker_cocoa.h"
#include "chrome/browser/ui/cocoa/web_intent_picker_test_base.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"

class WebIntentPickerSheetControllerTest
    : public TabContentsWrapperTestHarness,
      public WebIntentPickerTestBase {
 public:
  WebIntentPickerSheetControllerTest()
      : ui_thread_(content::BrowserThread::UI, MessageLoopForUI::current()) {}

  virtual ~WebIntentPickerSheetControllerTest() {
    message_loop_.RunAllPending();
  }

  virtual void TearDown() {
    if (picker_.get()) {
      EXPECT_CALL(delegate_, OnCancelled());
      EXPECT_CALL(delegate_, OnClosing());

      [controller_ cancelOperation:controller_];
      // Closing |controller_| destroys |picker_|.
      ignore_result(picker_.release());
    }
    TabContentsWrapperTestHarness::TearDown();
  }

  void CreatePicker() {
    picker_.reset(new WebIntentPickerCocoa());
    picker_->delegate_ = &delegate_;
    picker_->model_ = &model_;
    window_ = nil;
    controller_ = nil;
  }

  // Checks the controller's window for the requisite subviews and icons.
  void CheckWindow(size_t icon_count) {
    NSArray* flip_views = [[window_ contentView] subviews];

    // Check for proper firstResponder.
    ASSERT_EQ(controller_, [window_ firstResponder]);

    // Expect 1 subview - the flip view.
    ASSERT_EQ(1U, [flip_views count]);

    NSArray* views = [[flip_views objectAtIndex:0] subviews];

    // 4 + |icon_count| subviews - icon, header text, close button,
    // |icon_count| buttons, and a CWS link.
    ASSERT_EQ(4U + icon_count, [views count]);

    ASSERT_TRUE([[views objectAtIndex:0] isKindOfClass:[NSTextField class]]);
    ASSERT_TRUE([[views objectAtIndex:1] isKindOfClass:[NSImageView class]]);
    ASSERT_TRUE([[views objectAtIndex:2] isKindOfClass:
        [HoverCloseButton class]]);
    for(NSUInteger i = 0; i < icon_count; ++i) {
      ASSERT_TRUE([[views objectAtIndex:3 + i] isKindOfClass:
          [NSButton class]]);
    }

    // Verify the close button
    NSButton* close_button = static_cast<NSButton*>([views objectAtIndex:2]);
    CheckButton(close_button, @selector(cancelOperation:));

    // Verify the Chrome Web Store button.
    NSButton* button = static_cast<NSButton*>([views lastObject]);
    ASSERT_TRUE([button isKindOfClass:[NSButton class]]);
    EXPECT_TRUE([[button cell] isKindOfClass:[HyperlinkButtonCell class]]);
    CheckButton(button, @selector(showChromeWebStore:));

    // Verify buttons pointing to services.
    for(NSUInteger i = 0; i < icon_count; ++i) {
      NSButton* button = [views objectAtIndex:3 + i];
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

  content::TestBrowserThread ui_thread_;
};

TEST_F(WebIntentPickerSheetControllerTest, EmptyBubble) {
  CreateBubble(contents_wrapper());

  CheckWindow(/*icon_count=*/0);
}

TEST_F(WebIntentPickerSheetControllerTest, PopulatedBubble) {
  CreateBubble(contents_wrapper());

  WebIntentPickerModel model;
  model.AddInstalledService(string16(), GURL(),
      WebIntentPickerModel::DISPOSITION_WINDOW);
  model.AddInstalledService(string16(), GURL(),
      WebIntentPickerModel::DISPOSITION_WINDOW);

  [controller_ performLayoutWithModel:&model];

  CheckWindow(/*icon_count=*/2);
}

TEST_F(WebIntentPickerSheetControllerTest, OnCancelledWillSignalClose) {
  CreatePicker();

  EXPECT_CALL(delegate_, OnCancelled());
  EXPECT_CALL(delegate_, OnClosing());
  picker_->OnCancelled();

  ignore_result(picker_.release());  // Closing |picker_| will destruct it.
}

TEST_F(WebIntentPickerSheetControllerTest, SuggestionView) {
  CreateBubble(contents_wrapper());

  WebIntentPickerModel model;

  model.AddSuggestedExtension(string16(), string16(), 2.5);
  [controller_ performLayoutWithModel:&model];

  // Get subviews.
  NSArray* flip_views = [[window_ contentView] subviews];
  NSArray* main_views = [[flip_views objectAtIndex:0] subviews];

  // 4th object should be the suggestion view.
  ASSERT_TRUE([main_views count] > 3);
  ASSERT_TRUE([[main_views objectAtIndex:3] isKindOfClass:[NSView class]]);
  NSView* suggest_view = [main_views objectAtIndex:3];

  // There is exactly one subview, which contains the suggested item.
  ASSERT_EQ(1U, [[suggest_view subviews] count]);
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
