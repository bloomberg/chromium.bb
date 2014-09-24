// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_pending_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#import "chrome/browser/ui/cocoa/passwords/manage_password_item_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/save_password_refusal_combobox_model.h"
#include "chrome/grit/generated_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"

using namespace password_manager::mac::ui;

@interface ManagePasswordsBubblePendingViewController ()
- (void)onSaveClicked:(id)sender;
- (void)onNopeClicked:(id)sender;
- (void)onNeverForThisSiteClicked:(id)sender;
@end

@implementation ManagePasswordsBubblePendingViewController

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubblePendingViewDelegate>)delegate {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    model_ = model;
    delegate_ = delegate;
  }
  return self;
}

- (void)bubbleWillDisappear {
  // The "nope" drop-down won't be dismissed until the user chooses an option,
  // but if the bubble is dismissed (by cross-platform code) before the user
  // makes a choice, then the choice won't actually take any effect.
  [[nopeButton_ menu] cancelTrackingWithoutAnimation];
}

- (void)onSaveClicked:(id)sender {
  model_->OnSaveClicked();
  [delegate_ viewShouldDismiss];
}

- (void)onNopeClicked:(id)sender {
  model_->OnNopeClicked();
  [delegate_ viewShouldDismiss];
}

- (void)onNeverForThisSiteClicked:(id)sender {
  if (model_->best_matches().empty()) {
    // Skip confirmation if there are no existing passwords for this site.
    model_->OnNeverForThisSiteClicked();
    [delegate_ viewShouldDismiss];
  } else {
    [delegate_ passwordShouldNeverBeSavedOnSiteWithExistingPasswords];
  }
}

- (void)loadView {
  self.view = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];

  // -----------------------------------
  // |  Title                          |
  // |  -----------------------------  | (1 px border)
  // |    username   password          |
  // |  -----------------------------  | (1 px border)
  // |                [Save] [Nope v]  |
  // -----------------------------------

  // The bubble should be wide enough to fit the title row, the username and
  // password row, and the buttons row on one line each, but not smaller than
  // kDesiredBubbleWidth.

  // Create the elements and add them to the view.

  // Title.
  NSTextField* titleLabel =
      [self addTitleLabel:base::SysUTF16ToNSString(model_->title())];

  // Password item.
  // It should be at least as wide as the box without the padding.
  passwordItem_.reset([[ManagePasswordItemViewController alloc]
      initWithModel:model_
       passwordForm:model_->pending_credentials()
           position:password_manager::ui::FIRST_ITEM]);
  NSView* password = [passwordItem_ view];
  [self.view addSubview:password];

  // Save button.
  saveButton_.reset(
      [[self addButton:l10n_util::GetNSString(IDS_PASSWORD_MANAGER_SAVE_BUTTON)
                target:self
                action:@selector(onSaveClicked:)] retain]);

  // Nope combobox.
  SavePasswordRefusalComboboxModel comboboxModel;
  nopeButton_.reset([[BubbleCombobox alloc] initWithFrame:NSZeroRect
                                                pullsDown:YES
                                                    model:&comboboxModel]);
  NSMenuItem* nopeItem =
      [nopeButton_ itemAtIndex:SavePasswordRefusalComboboxModel::INDEX_NOPE];
  [nopeItem setTarget:self];
  [nopeItem setAction:@selector(onNopeClicked:)];
  NSMenuItem* neverItem = [nopeButton_
      itemAtIndex:SavePasswordRefusalComboboxModel::INDEX_NEVER_FOR_THIS_SITE];
  [neverItem setTarget:self];
  [neverItem setAction:@selector(onNeverForThisSiteClicked:)];

  // Insert a dummy row at the top and set its title to the desired title.
  // Must be done in two steps to prevent NSPopUpButton from removing
  // duplicates.
  [nopeButton_ insertItemWithTitle:@"" atIndex:0];
  [[nopeButton_ itemAtIndex:0]
      setTitle:l10n_util::GetNSString(IDS_PASSWORD_MANAGER_CANCEL_BUTTON)];

  [nopeButton_ sizeToFit];
  [self.view addSubview:nopeButton_.get()];

  // Compute the bubble width using the password item.
  const CGFloat contentWidth =
      kFramePadding + NSWidth([password frame]) + kFramePadding;
  const CGFloat width = std::max(kDesiredBubbleWidth, contentWidth);

  // Layout the elements, starting at the bottom and moving up.

  // Buttons go on the bottom row and are right-aligned.
  // Start with [Save].
  CGFloat curX = width - kFramePadding - NSWidth([saveButton_ frame]);
  CGFloat curY = kFramePadding;
  [saveButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // [Save] goes to the left of [Nope].
  curX -= kRelatedControlHorizontalPadding + NSWidth([nopeButton_ frame]);
  curY = kFramePadding +
         (NSHeight([saveButton_ frame]) - NSHeight([nopeButton_ frame])) / 2.0;
  [nopeButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // Password item goes on the next row and is shifted right.
  curX = kFramePadding;
  curY = NSMaxY([saveButton_ frame]) + kUnrelatedControlVerticalPadding;
  [password setFrameOrigin:NSMakePoint(curX, curY)];

  // Title goes at the top after some padding.
  curY = NSMaxY([password frame]) + kUnrelatedControlVerticalPadding;
  [titleLabel setFrameOrigin:NSMakePoint(curX, curY)];

  // Update the bubble size.
  const CGFloat height = NSMaxY([titleLabel frame]) + kFramePadding;
  [self.view setFrame:NSMakeRect(0, 0, width, height)];
}

@end

@implementation ManagePasswordsBubblePendingViewController (Testing)

- (NSButton*)saveButton {
  return saveButton_.get();
}

- (BubbleCombobox*)nopeButton {
  return nopeButton_.get();
}

@end
