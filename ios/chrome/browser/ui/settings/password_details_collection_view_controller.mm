// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password_details_collection_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/settings/cells/password_details_item.h"
#import "ios/chrome/browser/ui/settings/reauthentication_module.h"
#import "ios/chrome/browser/ui/settings/save_passwords_collection_view_controller.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSite = kSectionIdentifierEnumZero,
  SectionIdentifierUsername,
  SectionIdentifierPassword,
  SectionIdentifierDelete,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeSite,
  ItemTypeCopySite,
  ItemTypeUsername,
  ItemTypeCopyUsername,
  ItemTypePassword,
  ItemTypeShowHide,
  ItemTypeCopyPassword,
  ItemTypeDelete,
};

}  // namespace

@interface PasswordDetailsCollectionViewController () {
  // The username to which the saved password belongs.
  NSString* _username;
  // The saved password.
  NSString* _password;
  // The origin site of the saved credential.
  NSString* _site;
  // Whether the password is shown in plain text form or in obscured form.
  BOOL _plainTextPasswordShown;
  // The password form.
  autofill::PasswordForm _passwordForm;
  // Instance of the parent view controller needed in order to update the
  // password list when a password is deleted.
  __weak id<PasswordDetailsCollectionViewControllerDelegate> _weakDelegate;
  // Module containing the reauthentication mechanism for viewing and copying
  // passwords.
  __weak id<ReauthenticationProtocol> _weakReauthenticationModule;
  // The password item.
  PasswordDetailsItem* _passwordItem;
}

@end

@implementation PasswordDetailsCollectionViewController

- (instancetype)
  initWithPasswordForm:(autofill::PasswordForm)passwordForm
              delegate:
                  (id<PasswordDetailsCollectionViewControllerDelegate>)delegate
reauthenticationModule:(id<ReauthenticationProtocol>)reauthenticationModule
              username:(NSString*)username
              password:(NSString*)password
                origin:(NSString*)origin {
  DCHECK(delegate);
  DCHECK(reauthenticationModule);
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    _weakDelegate = delegate;
    _weakReauthenticationModule = reauthenticationModule;
    _passwordForm = passwordForm;
    _username = [username copy];
    _password = [password copy];
    _site = base::SysUTF8ToNSString(_passwordForm.origin.spec());
    self.title =
        [PasswordDetailsCollectionViewController simplifyOrigin:origin];
    self.collectionViewAccessibilityIdentifier =
        @"PasswordDetailsCollectionViewController";
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(hidePassword)
                          name:UIApplicationDidEnterBackgroundNotification
                        object:nil];

    [self loadModel];
  }
  return self;
}

+ (NSString*)simplifyOrigin:(NSString*)origin {
  NSString* originWithoutScheme = nil;
  if (![origin rangeOfString:@"://"].length) {
    originWithoutScheme = origin;
  } else {
    originWithoutScheme =
        [[origin componentsSeparatedByString:@"://"] objectAtIndex:1];
  }
  return
      [[originWithoutScheme componentsSeparatedByString:@"/"] objectAtIndex:0];
}

#pragma mark - SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSite];
  CollectionViewTextItem* siteHeader =
      [[CollectionViewTextItem alloc] initWithType:ItemTypeHeader];
  siteHeader.text = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  [model setHeader:siteHeader forSectionWithIdentifier:SectionIdentifierSite];
  PasswordDetailsItem* siteItem =
      [[PasswordDetailsItem alloc] initWithType:ItemTypeSite];
  siteItem.text = _site;
  siteItem.showingText = YES;
  [model addItem:siteItem toSectionWithIdentifier:SectionIdentifierSite];
  [model addItem:[self siteCopyButtonItem]
      toSectionWithIdentifier:SectionIdentifierSite];

  [model addSectionWithIdentifier:SectionIdentifierUsername];
  CollectionViewTextItem* usernameHeader =
      [[CollectionViewTextItem alloc] initWithType:ItemTypeHeader];
  usernameHeader.text =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  usernameHeader.textColor = [[MDCPalette greyPalette] tint500];
  [model setHeader:usernameHeader
      forSectionWithIdentifier:SectionIdentifierUsername];
  PasswordDetailsItem* usernameItem =
      [[PasswordDetailsItem alloc] initWithType:ItemTypeUsername];
  usernameItem.text = _username;
  usernameItem.showingText = YES;
  [model addItem:usernameItem
      toSectionWithIdentifier:SectionIdentifierUsername];
  [model addItem:[self usernameCopyButtonItem]
      toSectionWithIdentifier:SectionIdentifierUsername];

  [model addSectionWithIdentifier:SectionIdentifierPassword];
  CollectionViewTextItem* passwordHeader =
      [[CollectionViewTextItem alloc] initWithType:ItemTypeHeader];
  passwordHeader.text =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
  passwordHeader.textColor = [[MDCPalette greyPalette] tint500];
  [model setHeader:passwordHeader
      forSectionWithIdentifier:SectionIdentifierPassword];
  _passwordItem = [[PasswordDetailsItem alloc] initWithType:ItemTypePassword];
  _passwordItem.text = _password;
  _passwordItem.showingText = NO;
  [model addItem:_passwordItem
      toSectionWithIdentifier:SectionIdentifierPassword];

  // TODO(crbug.com/159166): Change the style of the buttons once there are
  // final mocks.
  [model addItem:[self showHidePasswordButtonItem]
      toSectionWithIdentifier:SectionIdentifierPassword];
  [model addItem:[self passwordCopyButtonItem]
      toSectionWithIdentifier:SectionIdentifierPassword];

  [model addSectionWithIdentifier:SectionIdentifierDelete];
  [model addItem:[self deletePasswordButtonItem]
      toSectionWithIdentifier:SectionIdentifierDelete];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - Items

- (CollectionViewItem*)siteCopyButtonItem {
  CollectionViewTextItem* item =
      [[CollectionViewTextItem alloc] initWithType:ItemTypeCopySite];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_COPY_BUTTON);
  item.textColor = [[MDCPalette cr_bluePalette] tint500];
  // Accessibility label adds the header to the text, so that accessibility
  // users do not have to rely on the visual grouping to understand which part
  // of the credential is being copied.
  item.accessibilityLabel = [NSString
      stringWithFormat:@"%@: %@",
                       l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE),
                       l10n_util::GetNSString(
                           IDS_IOS_SETTINGS_SITE_COPY_BUTTON)];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

- (CollectionViewItem*)usernameCopyButtonItem {
  CollectionViewTextItem* item =
      [[CollectionViewTextItem alloc] initWithType:ItemTypeCopyUsername];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON);
  item.textColor = [[MDCPalette cr_bluePalette] tint500];
  // Accessibility label adds the header to the text, so that accessibility
  // users do not have to rely on the visual grouping to understand which part
  // of the credential is being copied.
  item.accessibilityLabel =
      [NSString stringWithFormat:@"%@: %@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME),
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON)];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

- (CollectionViewItem*)passwordCopyButtonItem {
  CollectionViewTextItem* item =
      [[CollectionViewTextItem alloc] initWithType:ItemTypeCopyPassword];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_COPY_BUTTON);
  item.textColor = [[MDCPalette cr_bluePalette] tint500];
  // Accessibility label adds the header to the text, so that accessibility
  // users do not have to rely on the visual grouping to understand which part
  // of the credential is being copied.
  item.accessibilityLabel =
      [NSString stringWithFormat:@"%@: %@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD),
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_PASSWORD_COPY_BUTTON)];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

- (CollectionViewItem*)showHidePasswordButtonItem {
  CollectionViewTextItem* item =
      [[CollectionViewTextItem alloc] initWithType:ItemTypeShowHide];
  item.text = [self showHideButtonText];
  item.textColor = [[MDCPalette cr_bluePalette] tint500];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

- (CollectionViewItem*)deletePasswordButtonItem {
  CollectionViewTextItem* item =
      [[CollectionViewTextItem alloc] initWithType:ItemTypeDelete];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON);
  item.textColor = [[MDCPalette cr_redPalette] tint500];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

#pragma mark - Actions

- (void)copySite {
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  generalPasteboard.string = _site;
  [self showCopyResultToast:l10n_util::GetNSString(
                                IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE)];
}

- (void)copyUsername {
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  generalPasteboard.string = _username;
  [self showCopyResultToast:l10n_util::GetNSString(
                                IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE)];
}

- (NSString*)showHideButtonText {
  if (_plainTextPasswordShown) {
    return l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON);
  }
  return l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
}

// Changes the text on the Show/Hide button appropriately according to
// |_plainTextPasswordShown|.
- (void)toggleShowHideButton {
  CollectionViewModel* model = self.collectionViewModel;
  NSIndexPath* path = [model indexPathForItemType:ItemTypeShowHide
                                sectionIdentifier:SectionIdentifierPassword];
  CollectionViewTextItem* item =
      base::mac::ObjCCastStrict<CollectionViewTextItem>(
          [model itemAtIndexPath:path]);
  item.text = [self showHideButtonText];
  item.textColor = [[MDCPalette cr_bluePalette] tint500];
  [self reconfigureCellsForItems:@[ item ]];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (void)showPassword {
  if (_plainTextPasswordShown) {
    return;
  }

  if ([_weakReauthenticationModule canAttemptReauth]) {
    __weak PasswordDetailsCollectionViewController* weakSelf = self;
    void (^showPasswordHandler)(BOOL) = ^(BOOL success) {
      PasswordDetailsCollectionViewController* strongSelf = weakSelf;
      if (!strongSelf || !success)
        return;
      PasswordDetailsItem* passwordItem = strongSelf->_passwordItem;
      passwordItem.showingText = YES;
      [strongSelf reconfigureCellsForItems:@[ passwordItem ]];
      [[strongSelf collectionView].collectionViewLayout invalidateLayout];
      strongSelf->_plainTextPasswordShown = YES;
      [strongSelf toggleShowHideButton];
    };

    [_weakReauthenticationModule
        attemptReauthWithLocalizedReason:
            l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW)
                                 handler:showPasswordHandler];
  }
}

- (void)hidePassword {
  if (!_plainTextPasswordShown) {
    return;
  }
  _passwordItem.showingText = NO;
  [self reconfigureCellsForItems:@[ _passwordItem ]];
  [self.collectionView.collectionViewLayout invalidateLayout];
  _plainTextPasswordShown = NO;
  [self toggleShowHideButton];
}

- (void)copyPassword {
  // If the password is displayed in plain text, there is no need to
  // re-authenticate the user when copying the password because they are already
  // granted access to it.
  if (_plainTextPasswordShown) {
    UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
    generalPasteboard.string = _password;
    TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
    [self
        showCopyResultToast:l10n_util::GetNSString(
                                IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE)];
  } else if ([_weakReauthenticationModule canAttemptReauth]) {
    __weak PasswordDetailsCollectionViewController* weakSelf = self;
    void (^copyPasswordHandler)(BOOL) = ^(BOOL success) {
      PasswordDetailsCollectionViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      if (success) {
        UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
        generalPasteboard.string = strongSelf->_password;
        TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
        [strongSelf showCopyResultToast:
                        l10n_util::GetNSString(
                            IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE)];
      } else {
        TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeError);
        [strongSelf showCopyResultToast:
                        l10n_util::GetNSString(
                            IDS_IOS_SETTINGS_PASSWORD_WAS_NOT_COPIED_MESSAGE)];
      }
    };
    [_weakReauthenticationModule
        attemptReauthWithLocalizedReason:
            l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_COPY)
                                 handler:copyPasswordHandler];
  }
}

- (void)showCopyResultToast:(NSString*)message {
  // TODO(crbug.com/159166): Route this through some delegate API to be able
  // to mock it in the unittest, and avoid having an EGTest just for that?
  MDCSnackbarMessage* copyPasswordResultMessage =
      [MDCSnackbarMessage messageWithText:message];
  copyPasswordResultMessage.category = @"PasswordsSnackbarCategory";
  [MDCSnackbarManager showMessage:copyPasswordResultMessage];
}

- (void)deletePassword {
  [_weakDelegate deletePassword:_passwordForm];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeCopySite:
      [self copySite];
      break;
    case ItemTypeCopyUsername:
      [self copyUsername];
      break;
    case ItemTypeShowHide:
      if (_plainTextPasswordShown) {
        [self hidePassword];
      } else {
        [self showPassword];
      }
      break;
    case ItemTypeCopyPassword:
      [self copyPassword];
      break;
    case ItemTypeDelete:
      [self deletePassword];
      break;
    default:
      break;
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeUsername:
    case ItemTypePassword:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

#pragma mark - ForTesting

- (void)setReauthenticationModule:
    (id<ReauthenticationProtocol>)reauthenticationModule {
  _weakReauthenticationModule = reauthenticationModule;
}

@end
