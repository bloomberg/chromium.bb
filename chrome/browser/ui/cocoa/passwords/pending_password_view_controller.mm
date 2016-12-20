// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#import "chrome/browser/ui/cocoa/passwords/pending_password_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/chrome_style.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"

@implementation PendingPasswordViewController

- (BOOL)textView:(NSTextView*)textView
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
  ManagePasswordsBubbleModel* model = [self model];
  if (model)
    model->OnBrandLinkClicked();
  [self.delegate viewShouldDismiss];
  return YES;
}

- (base::scoped_nsobject<NSButton>)newCloseButton {
  const int dimension = chrome_style::GetCloseButtonSize();
  NSRect frame = NSMakeRect(0, 0, dimension, dimension);
  base::scoped_nsobject<NSButton> button(
      [[WebUIHoverCloseButton alloc] initWithFrame:frame]);
  [button setAction:@selector(viewShouldDismiss)];
  [button setTarget:self.delegate];
  return button;
}

- (NSView*)createPasswordView {
  // Empty implementation, it should be implemented in child class.
  NOTREACHED();
  return nil;
}

- (NSArray*)createButtonsAndAddThemToView:(NSView*)view {
  // Empty implementation, it should be implemented in child class.
  NOTREACHED();
  return nil;
}

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // -----------------------------------
  // |  Title                        x |
  // |  username   password            |
  // |  Smart Lock  welcome (optional) |
  // |            [Button1] [Button2]  |
  // -----------------------------------

  // The title text depends on whether the user is signed in and therefore syncs
  // their password
  // Do you want [Google Smart Lock]/Chrome/Chromium to save password
  // for this site.

  // The bubble should be wide enough to fit the title row, the username and
  // password row, and the buttons row on one line each, but not smaller than
  // kDesiredBubbleWidth.

  // Create the elements and add them to the view.

  // Close button.
  closeButton_ = [self newCloseButton];
  [view addSubview:closeButton_];

  // Title.
  ManagePasswordsBubbleModel* model = [self model];
  HyperlinkTextView* titleView = TitleBubbleLabelWithLink(
      model->title(), model->title_brand_link_range(), self);

  // Force the text to wrap to fit in the bubble size.
  int titleRightPadding =
      2 * chrome_style::kCloseButtonPadding + NSWidth([closeButton_ frame]);
  int titleWidth = kDesiredBubbleWidth - kFramePadding - titleRightPadding;
  [titleView setVerticallyResizable:YES];
  [titleView setFrameSize:NSMakeSize(titleWidth, MAXFLOAT)];
  // Set the same text inset as in |passwordRow|.
  [[titleView textContainer] setLineFragmentPadding:kTitleTextInset];
  [titleView sizeToFit];

  [view addSubview:titleView];

  // Password item.
  // It should be at least as wide as the box without the padding.
  NSView* passwordRow = [self createPasswordView];
  if (passwordRow) {
    [view addSubview:passwordRow];
  }

  NSArray* buttons = [self createButtonsAndAddThemToView:view];

  // Compute the bubble width using the password item.
  const CGFloat contentWidth =
      kFramePadding + NSWidth([titleView frame]) + titleRightPadding;
  const CGFloat width = std::max(kDesiredBubbleWidth, contentWidth);

  // Layout the elements, starting at the bottom and moving up.

  // Buttons go on the bottom row and are right-aligned.
  // Start with [Save].
  CGFloat curX = width - kFramePadding + kRelatedControlHorizontalPadding;
  CGFloat curY = kFramePadding;

  for (NSButton* button in buttons) {
    curX -= kRelatedControlHorizontalPadding + NSWidth([button frame]);
    [button setFrameOrigin:NSMakePoint(curX, curY)];
  }

  curX = kFramePadding;
  curY = NSMaxY([buttons.firstObject frame]) + kUnrelatedControlVerticalPadding;

  if (passwordRow) {
    // Password item goes on the next row.
    [passwordRow setFrameOrigin:NSMakePoint(curX, curY)];

    // Title goes at the top after some padding.
    curY = NSMaxY([passwordRow frame]) + kUnrelatedControlVerticalPadding;
  }
  [titleView setFrameOrigin:NSMakePoint(curX, curY)];
  const CGFloat height = NSMaxY([titleView frame]) + kFramePadding;

  // The close button is in the corner.
  NSPoint closeButtonOrigin = NSMakePoint(
      width - NSWidth([closeButton_ frame]) - chrome_style::kCloseButtonPadding,
      height - NSHeight([closeButton_ frame]) -
          chrome_style::kCloseButtonPadding);
  [closeButton_ setFrameOrigin:closeButtonOrigin];

  // Update the bubble size.

  [view setFrame:NSMakeRect(0, 0, width, height)];

  [self setView:view];
}

- (ManagePasswordsBubbleModel*)model {
  return [self.delegate model];
}

@end

@implementation PendingPasswordViewController (Testing)

- (NSButton*)closeButton {
  return closeButton_.get();
}

@end
