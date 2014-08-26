// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_blacklist_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "grit/generated_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"

using namespace password_manager::mac::ui;

@interface ManagePasswordsBubbleBlacklistViewController ()
- (void)onDoneClicked:(id)sender;
- (void)onUndoBlacklistClicked:(id)sender;
@end

@implementation ManagePasswordsBubbleBlacklistViewController

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    model_ = model;
    delegate_ = delegate;
  }
  return self;
}

- (void)onDoneClicked:(id)sender {
  model_->OnDoneClicked();
  [delegate_ viewShouldDismiss];
}

- (void)onUndoBlacklistClicked:(id)sender {
  model_->OnUnblacklistClicked();
  [delegate_ viewShouldDismiss];
}

- (void)loadView {
  self.view = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];

  // -----------------------------------
  // |  Title                          |
  // |                                 |
  // |  Blacklisted!                   |
  // |                                 |
  // |                  [Done] [Undo]  |
  // -----------------------------------

  // Create the elements and add them to the view.

  // Title.
  NSTextField* titleLabel =
      [self addTitleLabel:base::SysUTF16ToNSString(model_->title())];

  // Blacklist explanation.
  NSTextField* explanationLabel =
      [self addLabel:l10n_util::GetNSString(IDS_MANAGE_PASSWORDS_BLACKLISTED)];

  // Done button.
  doneButton_.reset([[self addButton:l10n_util::GetNSString(IDS_DONE)
                              target:self
                              action:@selector(onDoneClicked:)] retain]);

  // Undo button.
  undoBlacklistButton_.reset([[self
      addButton:l10n_util::GetNSString(IDS_PASSWORD_MANAGER_UNBLACKLIST_BUTTON)
         target:self
         action:@selector(onUndoBlacklistClicked:)] retain]);

  // Compute the bubble width using the title and explanation labels.
  // The explanation label can wrap to multiple lines
  const CGFloat contentWidth =
      std::max(NSWidth([titleLabel frame]), kDesiredBubbleWidth);
  [explanationLabel setFrameSize:NSMakeSize(contentWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:explanationLabel];
  const CGFloat width = kFramePadding + contentWidth + kFramePadding;

  // Layout the elements, starting at the bottom and moving up.

  // Buttons go on the bottom row and are right-aligned.
  // Start with [Undo].
  CGFloat curX = width - kFramePadding - NSWidth([undoBlacklistButton_ frame]);
  CGFloat curY = kFramePadding;
  [undoBlacklistButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // [Done] goes to the left of [Undo].
  curX = NSMinX([undoBlacklistButton_ frame]) -
         kRelatedControlHorizontalPadding -
         NSWidth([doneButton_ frame]);
  [doneButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // Explanation label goes on the next row and is shifted right.
  curX = kFramePadding;
  curY = NSMaxY([doneButton_ frame]) + kUnrelatedControlVerticalPadding;
  [explanationLabel setFrameOrigin:NSMakePoint(curX, curY)];

  // Title goes at the top after some padding.
  curY = NSMaxY([explanationLabel frame]) + kUnrelatedControlVerticalPadding;
  [titleLabel setFrameOrigin:NSMakePoint(curX, curY)];

  // Update the bubble size.
  const CGFloat height =
      NSMaxY([titleLabel frame]) + kFramePadding;
  [self.view setFrame:NSMakeRect(0, 0, width, height)];
}

@end

@implementation ManagePasswordsBubbleBlacklistViewController (Testing)

- (NSButton*)doneButton {
  return doneButton_.get();
}

- (NSButton*)undoBlacklistButton {
  return undoBlacklistButton_.get();
}

@end

