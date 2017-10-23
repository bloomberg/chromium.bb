// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/save_pending_password_view_controller.h"

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
#include "ui/gfx/render_text.h"

namespace {

// Touch bar item identifiers.
NSString* const kEditTouchBarId = @"EDIT";
NSString* const kNeverTouchBarId = @"NEVER";
NSString* const kSaveTouchBarId = @"SAVE";

constexpr base::char16 kBulletChar = gfx::RenderText::kPasswordReplacementChar;

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

void FillPasswordPopup(const autofill::PasswordForm& form,
                       bool visible,
                       NSPopUpButton* button) {
  [button removeAllItems];
  for (const base::string16& possible_password : form.all_possible_passwords) {
    base::string16 text =
        visible ? possible_password
                : base::string16(possible_password.length(), kBulletChar);
    base::scoped_nsobject<NSMenuItem> newItem([[NSMenuItem alloc]
        initWithTitle:base::SysUTF16ToNSString(text)
               action:NULL
        keyEquivalent:[NSString string]]);
    [[button menu] addItem:newItem];
  }
  size_t index = std::distance(
      form.all_possible_passwords.begin(),
      find(form.all_possible_passwords.begin(),
           form.all_possible_passwords.end(), form.password_value));
  // Unlikely, but if we don't find the password in possible passwords,
  // we will set the default to first element.
  if (index == form.all_possible_passwords.size()) {
    [button selectItemAtIndex:0];
  } else {
    [button selectItemAtIndex:index];
  }
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

@interface SavePendingPasswordViewController () {
  base::scoped_nsobject<NSTextField> usernameField_;
  // The field contains the password or IDP origin for federated credentials.
  base::scoped_nsobject<NSTextField> passwordStaticField_;
  // The button that allows selection from the list.
  base::scoped_nsobject<NSPopUpButton> passwordSelectionField_;
  // Static label with the text "Password".
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

- (NSButton*)defaultButton {
  return saveButton_;
}

- (void)viewWillAppear {
  NSView* firstResponder = saveButton_.get();
  if ([usernameField_ isEditable] && [[usernameField_ stringValue] length] == 0)
    firstResponder = usernameField_.get();
  [[self.view window] setInitialFirstResponder:firstResponder];
  [[self.view window] recalculateKeyViewLoop];
}

- (void)onEyeClicked:(id)sender {
  if (!self.model)
    return;  // The view will be destroyed soon.
  bool visible = [passwordViewButton_ state] == NSOnState;
  const autofill::PasswordForm& form = self.model->pending_password();
  if (passwordSelectionField_) {
    NSInteger index = [passwordSelectionField_ indexOfSelectedItem];
    self.model->OnCredentialEdited(form.username_value,
                                   form.all_possible_passwords[index]);
    FillPasswordPopup(form, visible, passwordSelectionField_.get());
  } else {
    DCHECK(passwordStaticField_);
    base::string16 text =
        visible ? form.password_value
                : base::string16(form.password_value.length(), kBulletChar);
    [passwordStaticField_ setStringValue:base::SysUTF16ToNSString(text)];
  }
}

- (void)onSaveClicked:(id)sender {
  ManagePasswordsBubbleModel* model = self.model;
  if (model) {
    base::string16 new_username =
        base::SysNSStringToUTF16([usernameField_ stringValue]);
    base::string16 new_password = self.model->pending_password().password_value;
    if (passwordSelectionField_) {
      NSInteger index = [passwordSelectionField_ indexOfSelectedItem];
      new_password =
          self.model->pending_password().all_possible_passwords[index];
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
        passwordSelectionField_.reset(
            [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO]);
        FillPasswordPopup(form, false, passwordSelectionField_.get());
        [passwordSelectionField_ sizeToFit];
      } else {
        passwordStaticField_.reset([Label(
            base::string16(form.password_value.length(), kBulletChar)) retain]);
        // Overwrite the height of the password field because it's higher in the
        // editable mode.
        [passwordStaticField_
            setFrameSize:NSMakeSize(
                             NSWidth([passwordStaticField_ frame]),
                             std::max(NSHeight([passwordStaticField_ frame]),
                                      NSHeight([EditableField(
                                          form.password_value) frame])))];
      }
      if (!self.model->hide_eye_icon()) {
        passwordViewButton_.reset(
            [EyeIcon(self, @selector(onEyeClicked:)) retain]);
        [container addSubview:passwordViewButton_];
      }
    } else {
      passwordStaticField_.reset([PasswordLabel(form.password_value) retain]);
    }
  } else {
    base::string16 text = l10n_util::GetStringFUTF16(
        IDS_PASSWORDS_VIA_FEDERATION,
        base::UTF8ToUTF16(form.federation_origin.host()));
    passwordStaticField_.reset([Label(text) retain]);
  }
  NSView* textField = passwordStaticField_ ? passwordStaticField_.get()
                                           : passwordSelectionField_.get();
  DCHECK(textField);
  [container addSubview:textField];

  NSTextField* usernameText =
      Label(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USERNAME_LABEL));
  [container addSubview:usernameText];
  base::string16 passwordLabel =
      form.federation_origin.unique()
          ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSWORD_LABEL)
          : base::string16();
  passwordText_.reset([Label(passwordLabel) retain]);
  [container addSubview:passwordText_];

  // Layout the elements.
  CGFloat firstColumnSize =
      std::max(NSWidth([usernameText frame]), NSWidth([passwordText_ frame]));
  // Bottow row.
  CGFloat rowHeight =
      std::max(NSHeight([textField frame]), NSHeight([passwordText_ frame]));
  CGFloat curY = (rowHeight - NSHeight([passwordText_ frame])) / 2;
  [passwordText_ setFrameOrigin:NSMakePoint(firstColumnSize -
                                                NSWidth([passwordText_ frame]),
                                            curY)];
  CGFloat curX = NSMaxX([passwordText_ frame]) + kItemLabelSpacing;
  if (passwordSelectionField_) {
    // The pop-up button is center-aligned with the label.
    curY = (rowHeight - NSHeight([passwordSelectionField_ frame])) / 2;
  } else {
    // Password field is top-aligned with the label because it's not editable.
    curY = NSMaxY([passwordText_ frame]) - NSHeight([textField frame]);
  }
  [textField setFrameOrigin:NSMakePoint(curX, curY)];
  CGFloat remainingWidth = kDesiredRowWidth - NSMinX([textField frame]);
  if (passwordViewButton_) {
    // The eye icon should be right-aligned.
    curX = kDesiredRowWidth - NSWidth([passwordViewButton_ frame]);
    curY = (rowHeight - NSHeight([passwordViewButton_ frame])) / 2;
    [passwordViewButton_ setFrameOrigin:NSMakePoint(curX, curY)];
    remainingWidth -=
        (NSWidth([passwordViewButton_ frame]) + kItemLabelSpacing);
  }
  [textField
      setFrameSize:NSMakeSize(remainingWidth, NSHeight([textField frame]))];
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

- (NSTextField*)passwordStaticField {
  return passwordStaticField_.get();
}

- (NSPopUpButton*)passwordSelectionField {
  return passwordSelectionField_.get();
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
