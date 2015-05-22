// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_save_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/save_account_more_combobox_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using namespace password_manager::mac::ui;

@interface ManagePasswordsBubbleSaveViewController ()
- (void)onSaveClicked:(id)sender;
- (void)onNoThanksClicked:(id)sender;
- (void)onNeverForThisSiteClicked:(id)sender;
- (void)onShowSettingsClicked:(id)sender;
@end

@implementation ManagePasswordsBubbleSaveViewController

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubblePendingViewDelegate>)delegate {
  if (([super initWithDelegate:delegate])) {
    model_ = model;
  }
  return self;
}

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // -----------------------------------
  // |  Title                          |
  // |                                 |
  // |  [More v]    [No thanks] [Save] |
  // -----------------------------------

  // Title.
  NSTextField* titleLabel =
      [self addTitleLabel:base::SysUTF16ToNSString(model_->title())
                   toView:view];

  // "Save" button.
  saveButton_.reset([[self
      addButton:l10n_util::GetNSString(IDS_PASSWORD_MANAGER_SAVE_ACCOUNT_BUTTON)
         toView:view
         target:self
         action:@selector(onSaveClicked:)] retain]);

  // "No thanks" button.
  noThanksButton_.reset([[self
      addButton:
          l10n_util::GetNSString(
              IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SMART_LOCK_NO_THANKS_BUTTON)
         toView:view
         target:self
         action:@selector(onNoThanksClicked:)] retain]);

  // "More" combobox.
  SaveAccountMoreComboboxModel comboboxModel;
  moreButton_.reset([[BubbleCombobox alloc] initWithFrame:NSZeroRect
                                                pullsDown:YES
                                                    model:&comboboxModel]);
  NSMenuItem* neverItem = [moreButton_
      itemAtIndex:SaveAccountMoreComboboxModel::INDEX_NEVER_FOR_THIS_SITE];
  [neverItem setTarget:self];
  [neverItem setAction:@selector(onNeverForThisSiteClicked:)];
  NSMenuItem* settingsItem =
      [moreButton_ itemAtIndex:SaveAccountMoreComboboxModel::INDEX_SETTINGS];
  [settingsItem setTarget:self];
  [settingsItem setAction:@selector(onShowSettingsClicked:)];
  [moreButton_ sizeToFit];
  [view addSubview:moreButton_.get()];

  // Compute the bubble width from the button row.
  const CGFloat contentWidth = kFramePadding + NSWidth([saveButton_ frame]) +
                               kRelatedControlHorizontalPadding +
                               NSWidth([noThanksButton_ frame]) +
                               kUnrelatedControlVerticalPadding +
                               NSWidth([moreButton_ frame]) + kFramePadding;
  const CGFloat width = std::max(kDesiredBubbleWidth, contentWidth);

  // Layout the elements, starting at the bottom and moving up.

  // "Save" and "No Thanks" buttons go on the bottom row and are right-aligned.
  // Start with "Save".
  CGFloat curX = width - kFramePadding - NSWidth([saveButton_ frame]);
  CGFloat curY = kFramePadding;
  [saveButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // "Save" goes to the left of "Nope".
  curX -= kRelatedControlHorizontalPadding + NSWidth([noThanksButton_ frame]);
  [noThanksButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // "More" button is left-aligned.
  curX = kFramePadding;
  curY =
      kFramePadding +
      (NSHeight([saveButton_ frame]) - NSHeight([moreButton_ frame])) / 2.0;
  [moreButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // Title goes at the top after some padding.
  curY = NSMaxY([saveButton_ frame]) + kUnrelatedControlVerticalPadding;
  [titleLabel setFrameOrigin:NSMakePoint(curX, curY)];

  // Update the bubble size.
  const CGFloat height = NSMaxY([titleLabel frame]) + kFramePadding;
  [view setFrame:NSMakeRect(0, 0, width, height)];

  [self setView:view];
}

- (void)onSaveClicked:(id)sender {
  model_->OnSaveClicked();
  [delegate_ viewShouldDismiss];
}

- (void)onNoThanksClicked:(id)sender {
  model_->OnNopeClicked();
  [delegate_ viewShouldDismiss];
}

- (void)onNeverForThisSiteClicked:(id)sender {
  if (model_->local_credentials().empty()) {
    // Skip confirmation if there are no existing passwords for this site.
    model_->OnNeverForThisSiteClicked();
    [delegate_ viewShouldDismiss];
  } else {
    SEL selector =
        @selector(passwordShouldNeverBeSavedOnSiteWithExistingPasswords);
    if ([delegate_ respondsToSelector:selector])
      [delegate_ performSelector:selector];
  }
}

- (void)onShowSettingsClicked:(id)sender {
  model_->OnManageLinkClicked();
  [delegate_ viewShouldDismiss];
}

- (void)bubbleWillDisappear {
  // The "More" drop-down won't be dismissed until the user chooses an option,
  // but if the bubble is dismissed (by cross-platform code) before the user
  // makes a choice, then the choice won't actually take any effect.
  [[moreButton_ menu] cancelTrackingWithoutAnimation];
}

@end
