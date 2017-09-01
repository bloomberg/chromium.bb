// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bubble_anchor_helper_views.h"

#import <Cocoa/Cocoa.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

// Anchor offset in screen space (origin at bottom left of screen).
constexpr int kHorizOffset = 210;
constexpr int kVertOffset = 320;

class TestBubbleDialogDelegateView final
    : public views::BubbleDialogDelegateView {
 public:
  explicit TestBubbleDialogDelegateView(views::BubbleBorder::Arrow arrow)
      : BubbleDialogDelegateView(nullptr, arrow) {
    set_close_on_deactivate(false);
    set_shadow(views::BubbleBorder::NO_ASSETS);
    int screen_height = NSMaxY([[[NSScreen screens] firstObject] frame]);
    SetAnchorRect(gfx::Rect(kHorizOffset, screen_height - kVertOffset, 0, 0));
  }

  ~TestBubbleDialogDelegateView() override {}

  // BubbleDialogDelegateView overrides:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(200, 150);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBubbleDialogDelegateView);
};

NSWindow* ShowAnchoredBubble(NSWindow* parent,
                             views::BubbleBorder::Arrow arrow) {
  TestBubbleDialogDelegateView* bubble =
      new TestBubbleDialogDelegateView(arrow);
  bubble->set_parent_window([parent contentView]);
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->GetWidget()->Show();
  KeepBubbleAnchored(bubble);
  return bubble->GetWidget()->GetNativeWindow();
}

}  // namespace

using BubbleAnchorHelperViewsTest = views::ViewsTestBase;

// Test that KeepBubbleAnchored(..) actually keeps the bubble anchored upon a
// resize of the parent window.
TEST_F(BubbleAnchorHelperViewsTest, AnchoringFixed) {
  // Use MD anchoring since the arithmetic is simpler (no arrows).
  ui::test::MaterialDesignControllerTestAPI md_test_api(
      ui::MaterialDesignController::MATERIAL_NORMAL);
  md_test_api.SetSecondaryUiMaterial(true);

  // Released when closed.
  NSRect parent_frame = NSMakeRect(100, 200, 300, 400);
  NSWindow* parent =
      [[NSWindow alloc] initWithContentRect:parent_frame
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  [parent makeKeyAndOrderFront:nil];

  NSWindow* child = ShowAnchoredBubble(parent, views::BubbleBorder::TOP_RIGHT);

  // Anchored TOP_RIGHT, so Max X/Y should be anchored.
  EXPECT_EQ(kHorizOffset, NSMaxX([child frame]));
  EXPECT_EQ(kVertOffset, NSMaxY([child frame]));

  // Resize the parent from the top right.
  parent_frame.size.width += 50;
  parent_frame.size.height += 60;
  [parent setFrame:parent_frame display:YES animate:NO];
  EXPECT_EQ(kHorizOffset + 50, NSMaxX([child frame]));
  EXPECT_EQ(kVertOffset + 60, NSMaxY([child frame]));

  // Resize from the bottom left (no change).
  parent_frame.size.width -= 50;
  parent_frame.size.height -= 60;
  parent_frame = NSOffsetRect(parent_frame, 50, 60);
  [parent setFrame:parent_frame display:YES animate:NO];
  EXPECT_EQ(kHorizOffset + 50, NSMaxX([child frame]));
  EXPECT_EQ(kVertOffset + 60, NSMaxY([child frame]));

  // Move the window.
  parent_frame = NSOffsetRect(parent_frame, -50, -60);
  [parent setFrame:parent_frame display:YES animate:NO];
  EXPECT_EQ(kHorizOffset, NSMaxX([child frame]));
  EXPECT_EQ(kVertOffset, NSMaxY([child frame]));
  [child close];

  child = ShowAnchoredBubble(parent, views::BubbleBorder::TOP_LEFT);

  // Anchored TOP_LEFT, so MinX / MaxY should be anchored.
  EXPECT_EQ(kHorizOffset, NSMinX([child frame]));
  EXPECT_EQ(kVertOffset, NSMaxY([child frame]));

  // Resize the parent from right (no change).
  parent_frame.size.width += 50;
  [parent setFrame:parent_frame display:YES animate:NO];
  EXPECT_EQ(kHorizOffset, NSMinX([child frame]));
  EXPECT_EQ(kVertOffset, NSMaxY([child frame]));

  // Resize the parent from the left.
  parent_frame.size.width -= 50;
  parent_frame.origin.x += 50;
  [parent setFrame:parent_frame display:YES animate:NO];
  EXPECT_EQ(kHorizOffset + 50, NSMinX([child frame]));
  EXPECT_EQ(kVertOffset, NSMaxY([child frame]));

  [parent close];  // Takes |child| with it.
}
