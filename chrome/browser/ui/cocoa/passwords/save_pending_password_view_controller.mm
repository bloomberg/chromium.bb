// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/save_pending_password_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/common/password_manager_features.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Touch bar item identifiers.
NSString* const kEditTouchBarId = @"EDIT";
NSString* const kNeverTouchBarId = @"NEVER";
NSString* const kSaveTouchBarId = @"SAVE";

void InitEditableLabel(NSTextField* textField, const base::string16& text) {
  [textField setStringValue:base::SysUTF16ToNSString(text)];
  [textField setEditable:YES];
  [textField setSelectable:YES];
  [textField setFont:LabelFont()];
  [textField setBordered:YES];
  [textField setBezeled:YES];
  [[textField cell] setUsesSingleLineMode:YES];
}

NSTextField* EditableField(const base::string16& text) {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  InitEditableLabel(textField.get(), text);
  [textField sizeToFit];
  return textField.autorelease();
}

void FillPasswordCombobox(const autofill::PasswordForm& form,
                          bool visible,
                          NSComboBox* combobox) {
  [combobox removeAllItems];
  for (const base::string16& possible_password : form.all_possible_passwords) {
    [combobox
        addItemWithObjectValue:base::SysUTF16ToNSString(
                                   visible
                                       ? possible_password
                                       : base::string16(
                                             possible_password.length(), '*'))];
  }
  [combobox setEditable:visible];
  [combobox
      setStringValue:base::SysUTF16ToNSString(
                         visible ? form.password_value
                                 : base::string16(form.password_value.length(),
                                                  '*'))];
  size_t index = std::distance(
      form.all_possible_passwords.begin(),
      std::find(form.all_possible_passwords.begin(),
                form.all_possible_passwords.end(), form.password_value));
  if (index != form.all_possible_passwords.size())
    [combobox selectItemAtIndex:index];
}

NSComboBox* PasswordCombobox(const autofill::PasswordForm& form) {
  base::scoped_nsobject<NSComboBox> textField(
      [[NSComboBox alloc] initWithFrame:NSZeroRect]);
  FillPasswordCombobox(form, false, textField);
  [textField sizeToFit];
  return textField.autorelease();
}

NSButton* EyeIcon(id target, SEL action) {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [button setAction:action];
  [button setTarget:target];
  [[button cell] setHighlightsBy:NSNoCellMask];
  [button setButtonType:NSSwitchButton];
  [button setTitle:@""];
  [button setAlternateTitle:@""];
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  [button setImage:bundle.GetImageNamed(IDR_SHOW_PASSWORD_HOVER).ToNSImage()];
  [button setAlternateImage:bundle.GetImageNamed(IDR_HIDE_PASSWORD_HOVER)
                                .ToNSImage()];
  NSString* passwordViewTitle =
      l10n_util::GetNSString(IDS_MANAGE_PASSWORDS_SHOW_PASSWORD);
  [[button cell] accessibilitySetOverrideValue:passwordViewTitle
                                  forAttribute:NSAccessibilityTitleAttribute];
  [button setToolTip:passwordViewTitle];
  [button sizeToFit];
  return button.autorelease();
}

}  // end namespace

@interface SavePendingPasswordViewController ()<NSComboBoxDelegate> {
  base::scoped_nsobject<NSTextField> usernameField_;
  // The field contains the password or IDP origin for federated credentials.
  base::scoped_nsobject<NSTextField> passwordField_;
  base::scoped_nsobject<NSTextField> passwordText_;
  base::scoped_nsobject<NSButton> passwordViewButton_;
  base::scoped_nsobject<NSButton> saveButton_;
  base::scoped_nsobject<NSButton> neverButton_;
}
- (void)onEyeClicked:(id)sender;
- (void)onSaveClicked:(id)sender;
- (void)onNeverForThisSiteClicked:(id)sender;
@end

@implementation SavePendingPasswordViewController

- (void)dealloc {
  [passwordField_ setDelegate:nil];
  [super dealloc];
}

- (NSButton*)defaultButton {
  return saveButton_;
}

- (void)onEyeClicked:(id)sender {
  if (!self.model)
    return;  // The view will be destroyed soon.
  bool visible = [passwordViewButton_ state] == NSOnState;
  const autofill::PasswordForm& form = self.model->pending_password();
  if (!visible) {
    // The previous state was editable. Save the current result.
    self.model->OnCredentialEdited(
        form.username_value,
        base::SysNSStringToUTF16([passwordField_ stringValue]));
  }
  NSComboBox* combobox = base::mac::ObjCCast<NSComboBox>(passwordField_.get());
  if (combobox) {
    FillPasswordCombobox(self.model->pending_password(), visible, combobox);
  } else {
    NSRect oldFrame = [passwordField_ frame];
    CGFloat offsetY = 0;
    if (visible) {
      InitEditableLabel(passwordField_.get(), form.password_value);
      offsetY = NSMidY([passwordText_ frame]) - NSMidY(oldFrame);
    } else {
      InitLabel(passwordField_.get(),
                base::string16(form.password_value.length(), '*'));
      offsetY = NSMaxY([passwordText_ frame]) - NSMaxY(oldFrame);
    }
    [passwordField_ setFrame:NSOffsetRect(oldFrame, 0, offsetY)];
  }
  [[self.view window]
      makeFirstResponder:(visible ? passwordField_.get() : saveButton_.get())];
}

- (void)onSaveClicked:(id)sender {
  ManagePasswordsBubbleModel* model = self.model;
  if (model) {
    base::string16 new_username =
        base::SysNSStringToUTF16([usernameField_ stringValue]);
    base::string16 new_password = self.model->pending_password().password_value;
    if ([passwordViewButton_ state] == NSOnState) {
      // Update the password only if it is being edited. All other cases were
      // already handled.
      new_password = base::SysNSStringToUTF16([passwordField_ stringValue]);
    }
    model->OnCredentialEdited(std::move(new_username), std::move(new_password));
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

- (void)comboBoxSelectionDidChange:(NSNotification*)notification {
  if (!self.model)
    return;  // The view will be destroyed soon.
  NSInteger index = [base::mac::ObjCCastStrict<NSComboBox>(passwordField_)
      indexOfSelectedItem];
  const std::vector<base::string16>& passwords =
      self.model->pending_password().all_possible_passwords;
  if (index >= 0 && static_cast<size_t>(index) < passwords.size()) {
    self.model->OnCredentialEdited(
        self.model->pending_password().username_value, passwords[index]);
  }
}

- (NSView*)createPasswordView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);

  // Create the elements.
  bool enableUsernameEditing = base::FeatureList::IsEnabled(
      password_manager::features::kEnableUsernameCorrection);
  const autofill::PasswordForm& form = self.model->pending_password();
  if (enableUsernameEditing)
    usernameField_.reset([EditableField(form.username_value) retain]);
  else
    usernameField_.reset([Label(GetDisplayUsername(form)) retain]);
  [container addSubview:usernameField_];

  bool enablePasswordEditing = base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordSelection);
  if (form.federation_origin.unique()) {
    if (enablePasswordEditing) {
      if (form.all_possible_passwords.size() > 1) {
        passwordField_.reset([PasswordCombobox(form) retain]);
        [passwordField_ setDelegate:self];
      } else {
        passwordField_.reset(
            [Label(base::string16(form.password_value.length(), '*')) retain]);
        // Overwrite the height of the password field because it's higher in the
        // editable mode.
        [passwordField_
            setFrameSize:NSMakeSize(
                             NSWidth([passwordField_ frame]),
                             std::max(NSHeight([passwordField_ frame]),
                                      NSHeight([EditableField(
                                          form.username_value) frame])))];
      }
      if (!self.model->hide_eye_icon()) {
        passwordViewButton_.reset(
            [EyeIcon(self, @selector(onEyeClicked:)) retain]);
        [container addSubview:passwordViewButton_];
      }
    } else {
      passwordField_.reset([PasswordLabel(form.password_value) retain]);
    }
  } else {
    base::string16 text = l10n_util::GetStringFUTF16(
        IDS_PASSWORDS_VIA_FEDERATION,
        base::UTF8ToUTF16(form.federation_origin.host()));
    passwordField_.reset([Label(text) retain]);
  }
  [container addSubview:passwordField_];

  NSTextField* usernameText =
      Label(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USERNAME_LABEL));
  [container addSubview:usernameText];
  passwordText_.reset([Label(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSWORD_LABEL)) retain]);
  [container addSubview:passwordText_];

  // Layout the elements.
  CGFloat firstColumnSize =
      std::max(NSWidth([usernameText frame]), NSWidth([passwordText_ frame]));
  // Bottow row.
  CGFloat rowHeight = std::max(NSHeight([passwordField_ frame]),
                               NSHeight([passwordText_ frame]));
  CGFloat curY = (rowHeight - NSHeight([passwordText_ frame])) / 2;
  [passwordText_ setFrameOrigin:NSMakePoint(firstColumnSize -
                                                NSWidth([passwordText_ frame]),
                                            curY)];
  CGFloat curX = NSMaxX([passwordText_ frame]) + kItemLabelSpacing;
  // Password field is top-aligned with the label because it's not editable.
  curY = NSMaxY([passwordText_ frame]) - NSHeight([passwordField_ frame]);
  [passwordField_ setFrameOrigin:NSMakePoint(curX, curY)];
  CGFloat remainingWidth = kDesiredRowWidth - NSMinX([passwordField_ frame]);
  if (passwordViewButton_) {
    // The eye icon should be right-aligned.
    curX = kDesiredRowWidth - NSWidth([passwordViewButton_ frame]);
    curY = (rowHeight - NSHeight([passwordViewButton_ frame])) / 2;
    [passwordViewButton_ setFrameOrigin:NSMakePoint(curX, curY)];
    remainingWidth -=
        (NSWidth([passwordViewButton_ frame]) + kItemLabelSpacing);
  }
  [passwordField_ setFrameSize:NSMakeSize(remainingWidth,
                                          NSHeight([passwordField_ frame]))];
  // Next row.
  CGFloat rowY = rowHeight + kRelatedControlVerticalSpacing;
  rowHeight = std::max(NSHeight([usernameField_ frame]),
                       NSHeight([usernameText frame]));
  curX = firstColumnSize - NSWidth([usernameText frame]);
  curY = (rowHeight - NSHeight([usernameText frame])) / 2 + rowY;
  [usernameText setFrameOrigin:NSMakePoint(curX, curY)];
  curX = NSMaxX([usernameText frame]) + kItemLabelSpacing;
  curY = (rowHeight - NSHeight([usernameField_ frame])) / 2 + rowY;
  [usernameField_ setFrameOrigin:NSMakePoint(curX, curY)];
  remainingWidth = kDesiredRowWidth - NSMinX([usernameField_ frame]);
  [usernameField_ setFrameSize:NSMakeSize(remainingWidth,
                                          NSHeight([usernameField_ frame]))];
  // Update the frame.
  [container setFrameSize:NSMakeSize(NSMaxX([usernameField_ frame]),
                                     rowHeight + rowY)];
  cocoa_l10n_util::FlipAllSubviewsIfNecessary(container);
  return container.autorelease();
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

- (NSTextField*)usernameField {
  return usernameField_.get();
}

- (NSTextField*)passwordField {
  return passwordField_.get();
}

- (NSButton*)saveButton {
  return saveButton_.get();
}

- (NSButton*)neverButton {
  return neverButton_.get();
}

- (NSButton*)eyeButton {
  return passwordViewButton_.get();
}
@end
