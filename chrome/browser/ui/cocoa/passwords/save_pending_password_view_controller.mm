// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/save_pending_password_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/passwords/password_item_views.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_list_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_features.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Touch bar item identifiers.
NSString* const kEditTouchBarId = @"EDIT";
NSString* const kNeverTouchBarId = @"NEVER";
NSString* const kSaveTouchBarId = @"SAVE";

}  // end namespace

@interface SavePendingPasswordViewController ()<NSTextFieldDelegate>
- (void)onSaveClicked:(id)sender;
- (void)onNeverForThisSiteClicked:(id)sender;
- (void)onEditClicked:(id)sender;
- (void)refreshRow;
- (BOOL)disableEditMode;
- (BOOL)control:(NSControl*)control
               textView:(NSTextView*)textView
    doCommandBySelector:(SEL)commandSelector;
- (void)controlTextDidEndEditing:(NSNotification*)notification;
@end

@implementation SavePendingPasswordViewController

- (NSButton*)defaultButton {
  return saveButton_;
}

- (void)onSaveClicked:(id)sender {
  ManagePasswordsBubbleModel* model = self.model;
  if (model) {
    model->OnSaveClicked();
    if (model->ReplaceToShowPromotionIfNeeded()) {
      [self.delegate refreshBubble];
      return;
    }
  }
  [self.delegate viewShouldDismiss];
}

- (void)onNeverForThisSiteClicked:(id)sender {
  ManagePasswordsBubbleModel* model = self.model;
  if (model)
    model->OnNeverForThisSiteClicked();
  [self.delegate viewShouldDismiss];
}

- (void)onEditClicked:(id)sender {
  editMode_ = TRUE;
  [editButton_ setEnabled:FALSE];
  [self refreshRow];
}

// Escape character handler for editable username field.
- (BOOL)control:(NSControl*)control
               textView:(NSTextView*)textView
    doCommandBySelector:(SEL)commandSelector {
  if (commandSelector == @selector(cancelOperation:)) {
    return [self disableEditMode];
  }
  return FALSE;
}

// Focus handler for editable username field.
- (void)controlTextDidEndEditing:(NSNotification*)notification {
  PendingPasswordItemView* row =
      [[passwordItemContainer_ subviews] objectAtIndex:0];
  self.model->OnUsernameEdited(
      base::SysNSStringToUTF16([[row usernameField] stringValue]));
  [self disableEditMode];
}

- (BOOL)disableEditMode {
  if (editMode_) {
    editMode_ = FALSE;
    [editButton_ setEnabled:TRUE];
    [self refreshRow];
    [[passwordItemContainer_ window] setDefaultButtonCell:[saveButton_ cell]];
    return TRUE;
  }
  return FALSE;
}

- (NSView*)createPasswordView {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnableUsernameCorrection) &&
      self.model->pending_password().username_value.empty())
    return nil;
  base::scoped_nsobject<PendingPasswordItemView> item(
      [[PendingPasswordItemView alloc]
          initWithForm:self.model->pending_password()
              editMode:editMode_]);
  [item layoutWithFirstColumn:[item firstColumnWidth]
                 secondColumn:[item secondColumnWidth]];
  passwordItemContainer_.reset([[NSView alloc] initWithFrame:[item frame]]);
  [passwordItemContainer_ setSubviews:@[ item ]];
  return passwordItemContainer_;
}

- (void)refreshRow {
  base::scoped_nsobject<PendingPasswordItemView> item(
      [[PendingPasswordItemView alloc]
          initWithForm:self.model->pending_password()
              editMode:editMode_]);
  [item layoutWithFirstColumn:[item firstColumnWidth]
                 secondColumn:[item secondColumnWidth]];
  [passwordItemContainer_ setSubviews:@[ item ]];
  if (editMode_) {
    [[passwordItemContainer_ window] makeFirstResponder:[item usernameField]];
    [[item usernameField]
        setStringValue:base::SysUTF16ToNSString(
                           self.model->pending_password().username_value)];
    [[item usernameField].cell setUsesSingleLineMode:YES];
    [[item usernameField] setDelegate:self];
  }
}

- (NSArray*)createButtonsAndAddThemToView:(NSView*)view {
  // Save button.
  saveButton_.reset(
      [[self addButton:l10n_util::GetNSString(IDS_PASSWORD_MANAGER_SAVE_BUTTON)
                toView:view
                target:self
                action:@selector(onSaveClicked:)] retain]);

  // Never button.
  NSString* neverButtonText =
      l10n_util::GetNSString(IDS_PASSWORD_MANAGER_BUBBLE_BLACKLIST_BUTTON);
  neverButton_.reset(
      [[self addButton:neverButtonText
                toView:view
                target:self
                action:@selector(onNeverForThisSiteClicked:)] retain]);
  if (base::FeatureList::IsEnabled(
          password_manager::features::kEnableUsernameCorrection)) {
    // Edit button.
    editButton_.reset([[self
        addButton:l10n_util::GetNSString(IDS_PASSWORD_MANAGER_EDIT_BUTTON)
           toView:view
           target:self
           action:@selector(onEditClicked:)] retain]);
    return @[ saveButton_, neverButton_, editButton_ ];
  }
  return @[ saveButton_, neverButton_ ];
}

- (NSTouchBar*)makeTouchBar {
  if (!base::FeatureList::IsEnabled(features::kDialogTouchBar))
    return nil;

  base::scoped_nsobject<NSTouchBar> touchBar([[ui::NSTouchBar() alloc] init]);
  [touchBar setDelegate:self];

  NSArray* dialogItems = @[
    [self touchBarIdForItem:kEditTouchBarId],
    [self touchBarIdForItem:kNeverTouchBarId],
    [self touchBarIdForItem:kSaveTouchBarId]
  ];

  [touchBar setDefaultItemIdentifiers:dialogItems];
  return touchBar.autorelease();
}

- (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier
    API_AVAILABLE(macos(10.12.2)) {
  NSButton* button;
  if ([identifier isEqual:[self touchBarIdForItem:kEditTouchBarId]]) {
    button = [NSButton
        buttonWithTitle:l10n_util::GetNSString(IDS_PASSWORD_MANAGER_EDIT_BUTTON)
                 target:self
                 action:@selector(onEditClicked:)];
  } else if ([identifier isEqual:[self touchBarIdForItem:kNeverTouchBarId]]) {
    button = [NSButton
        buttonWithTitle:l10n_util::GetNSString(
                            IDS_PASSWORD_MANAGER_BUBBLE_BLACKLIST_BUTTON)
                 target:self
                 action:@selector(onNeverForThisSiteClicked:)];
  } else if ([identifier isEqual:[self touchBarIdForItem:kSaveTouchBarId]]) {
    button = ui::GetBlueTouchBarButton(
        l10n_util::GetNSString(IDS_PASSWORD_MANAGER_SAVE_BUTTON), self,
        @selector(onSaveClicked:));
  } else {
    return nil;
  }

  base::scoped_nsobject<NSCustomTouchBarItem> item(
      [[ui::NSCustomTouchBarItem() alloc] initWithIdentifier:identifier]);
  [item setView:button];
  return item.autorelease();
}

@end

@implementation SavePendingPasswordViewController (Testing)

- (NSButton*)editButton {
  return editButton_.get();
}

- (NSButton*)saveButton {
  return saveButton_.get();
}

- (NSButton*)neverButton {
  return neverButton_.get();
}

- (NSView*)passwordItemContainer {
  return passwordItemContainer_.get();
}
@end
