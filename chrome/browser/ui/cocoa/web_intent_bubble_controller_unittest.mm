// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/web_intent_bubble_controller.h"
#include "chrome/browser/ui/cocoa/web_intent_picker_cocoa.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"

namespace {

class FakeIntentPickerDelegate : public WebIntentPickerDelegate {
 public:
  virtual ~FakeIntentPickerDelegate() {}
  virtual void OnServiceChosen(
      size_t index, Disposition disposition) OVERRIDE {};
  virtual void OnInlineDispositionWebContentsCreated(
      content::WebContents* web_contents) OVERRIDE {}

  // Callback called when the user cancels out of the dialog.
  virtual void OnCancelled() OVERRIDE {};
  virtual void OnClosing() OVERRIDE {};
};

}  // namespace

class WebIntentBubbleControllerTest : public CocoaTest {
 public:
  virtual void TearDown() {
    // Do not animate out because that is hard to test around.
    [window_ setDelayOnClose:NO];
    [controller_ close];
    CocoaTest::TearDown();
  }

  void CreateBubble() {
    picker_.reset(new WebIntentPickerCocoa());
    picker_->delegate_ = &delegate_;
    NSPoint anchor=NSMakePoint(0,0);

    controller_ =
        [[WebIntentBubbleController alloc] initWithPicker:picker_.get()
                                             parentWindow:test_window()
                                               anchoredAt:anchor];
    window_ = static_cast<InfoBubbleWindow*>([controller_ window]);
    [controller_ showWindow:nil];
  }

  // Checks the controller's window for the requisite subviews and icons.
  void CheckWindow(size_t icon_count) {
    NSArray* flip_views = [[window_ contentView] subviews];

    // Expect 1 subview - the flip view.
    ASSERT_EQ(1U, [flip_views count]);

    NSArray* views = [[flip_views objectAtIndex:0] subviews];

    // 3 + |icon_count| subviews - Icon, Header text, |icon_count| buttons,
    // and a CWS link.
    ASSERT_EQ(3U + icon_count, [views count]);

    ASSERT_TRUE([[views objectAtIndex:0] isKindOfClass:[NSTextField class]]);
    ASSERT_TRUE([[views objectAtIndex:1] isKindOfClass:[NSImageView class]]);
    for(NSUInteger i = 0; i < icon_count; ++i) {
      ASSERT_TRUE([[views objectAtIndex:2 + i] isKindOfClass:[NSButton class]]);
    }

    // Verify the Chrome Web Store button.
    NSButton* button = static_cast<NSButton*>([views lastObject]);
    ASSERT_TRUE([button isKindOfClass:[NSButton class]]);
    EXPECT_TRUE([[button cell] isKindOfClass:[HyperlinkButtonCell class]]);
    CheckButton(button, @selector(showChromeWebStore:));

    // Verify buttons pointing to services.
    for(NSUInteger i = 0; i < icon_count; ++i) {
      NSButton* button = [views objectAtIndex:2 + i];
      CheckServiceButton(button, i);
    }

    EXPECT_EQ([window_ delegate], controller_);
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

  WebIntentBubbleController* controller_;  // Weak, owns self.
  InfoBubbleWindow* window_;  // Weak, owned by controller.
  scoped_ptr<WebIntentPickerCocoa> picker_;
  FakeIntentPickerDelegate delegate_;
};

TEST_F(WebIntentBubbleControllerTest, EmptyBubble) {
  CreateBubble();

  CheckWindow(/*icon_count=*/0);
}

TEST_F(WebIntentBubbleControllerTest, PopulatedBubble) {
  CreateBubble();

  WebIntentPickerModel model;
  model.AddItem(string16(),GURL(),WebIntentPickerModel::DISPOSITION_WINDOW);
  model.AddItem(string16(),GURL(),WebIntentPickerModel::DISPOSITION_WINDOW);

  [controller_ performLayoutWithModel:&model];

  CheckWindow(/*icon_count=*/2);
}
