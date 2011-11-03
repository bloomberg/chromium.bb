// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  virtual void OnServiceChosen(size_t index) {};

  // Callback called when the user cancels out of the dialog.
  virtual void OnCancelled() {};
  virtual void OnClosing() {};
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
    NSArray* views = [[window_ contentView] subviews];

    // 4 subviews - Icon, Header text, NSMatrix, CWS link.
    EXPECT_EQ(4U, [views count]);
    EXPECT_TRUE([[views objectAtIndex:0] isKindOfClass:[NSButton class]]);
    EXPECT_TRUE([[views objectAtIndex:1] isKindOfClass:[NSMatrix class]]);
    EXPECT_TRUE([[views objectAtIndex:2] isKindOfClass:[NSTextField class]]);
    EXPECT_TRUE([[views objectAtIndex:3] isKindOfClass:[NSImageView class]]);

    // Verify the Chrome Web Store button.
    NSButton* button = static_cast<NSButton*>([views objectAtIndex:0]);
    EXPECT_TRUE([[button cell] isKindOfClass:[HyperlinkButtonCell class]]);
    CheckButton(button, @selector(showChromeWebStore:));

    // Verify icons/buttons pointing to services.
    NSMatrix* icon_matrix = static_cast<NSMatrix*>([views objectAtIndex:1]);
    NSArray* cells = [icon_matrix cells];
    size_t cell_count = 0;
    for (NSButtonCell* cell in cells) {
      // Skip not populated cells
      if ([cell isKindOfClass:[NSImageCell class]])
        continue;

      ++cell_count;
      CheckButton(cell, @selector(invokeService:));
    }
    EXPECT_EQ(icon_count, cell_count);

    EXPECT_EQ([window_ delegate], controller_);
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
  [controller_ replaceImageAtIndex:2 withImage:nil];

  CheckWindow(/*icon_count=*/3);
}
