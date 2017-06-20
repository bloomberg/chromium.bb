// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/save_passwords_collection_view_controller.h"

#include <memory>
#include <vector>

#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/google/core/browser/google_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/password_details_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/reauthentication_module.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMessage = kSectionIdentifierEnumZero,
  SectionIdentifierSavePasswordsSwitch,
  SectionIdentifierSavedPasswords,
  SectionIdentifierBlacklist,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeManageAccount = kItemTypeEnumZero,
  ItemTypeHeader,
  ItemTypeSavePasswordsSwitch,
  ItemTypeSavedPassword,  // This is a repeated item type.
  ItemTypeBlacklisted,    // This is a repeated item type.
};

// TODO(crbug.com/669538): This function should be removed after original
// version of GetHumanReadableOrigin will be moved to affiliation_utils.
std::string GetHumanReadableOriginCopy(
    const autofill::PasswordForm& password_form) {
  password_manager::FacetURI facet_uri =
      password_manager::FacetURI::FromPotentiallyInvalidSpec(
          password_form.signon_realm);
  if (facet_uri.IsValidAndroidFacetURI())
    return facet_uri.scheme() + "://" + facet_uri.android_package_name();
  return base::UTF16ToUTF8(url_formatter::FormatUrl(password_form.origin));
}

}  // namespace

@interface SavePasswordsCollectionViewController (UsedBySavePasswordsConsumer)
// Callback called when the async request launched from
// |getLoginsFromPasswordStore| finishes.
- (void)onGetPasswordStoreResults:
    (const std::vector<std::unique_ptr<autofill::PasswordForm>>&)result;
@end

namespace password_manager {
// A bridge C++ class passing notification about finished password store
// requests to owning Obj-C class SavePasswordsCollectionViewController.
class SavePasswordsConsumer : public PasswordStoreConsumer {
 public:
  explicit SavePasswordsConsumer(
      SavePasswordsCollectionViewController* delegate);
  ~SavePasswordsConsumer() override;
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

 private:
  __unsafe_unretained SavePasswordsCollectionViewController* delegate_;  // weak
  DISALLOW_COPY_AND_ASSIGN(SavePasswordsConsumer);
};

SavePasswordsConsumer::SavePasswordsConsumer(
    SavePasswordsCollectionViewController* delegate)
    : delegate_(delegate) {}

SavePasswordsConsumer::~SavePasswordsConsumer() {}

void SavePasswordsConsumer::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  if (!results.empty())
    [delegate_ onGetPasswordStoreResults:results];
}
}  // namespace password_manager

// Use the type of the items to convey the Saved/Blacklisted status.
@interface SavedFormContentItem : CollectionViewTextItem
@end
@implementation SavedFormContentItem
@end
@interface BlacklistedFormContentItem : CollectionViewTextItem
@end
@implementation BlacklistedFormContentItem
@end

@interface SavePasswordsCollectionViewController ()<
    BooleanObserver,
    PasswordDetailsCollectionViewControllerDelegate,
    SuccessfulReauthTimeAccessor> {
  // The observable boolean that binds to the password manager setting state.
  // Saved passwords are only on if the password manager is enabled.
  PrefBackedBoolean* passwordManagerEnabled_;
  // The item related to the switch for the password manager setting.
  CollectionViewSwitchItem* savePasswordsItem_;
  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> passwordStore_;
  // A helper object for passing data about saved passwords from a finished
  // password store request to the SavePasswordsCollectionViewController.
  std::unique_ptr<password_manager::SavePasswordsConsumer>
      savedPasswordsConsumer_;
  // A helper object for passing data about blacklisted sites from a finished
  // password store request to the SavePasswordsCollectionViewController.
  std::unique_ptr<password_manager::SavePasswordsConsumer>
      blacklistPasswordsConsumer_;
  // The list of the user's saved passwords.
  std::vector<std::unique_ptr<autofill::PasswordForm>> savedForms_;
  // The list of the user's blacklisted sites.
  std::vector<std::unique_ptr<autofill::PasswordForm>> blacklistedForms_;
  // Deletion of password being asynchronous, and the method taking a reference
  // to the PasswordForm, the PasswordForm must outlive the calls to
  // RemoveLogin. This vector will ensure this.
  std::vector<std::unique_ptr<autofill::PasswordForm>> deletedForms_;
  // The current Chrome browser state.
  ios::ChromeBrowserState* browserState_;
  // Object storing the time of the previous successful re-authentication.
  // This is meant to be used by the |ReauthenticationModule| for keeping
  // re-authentications valid for a certain time interval within the scope
  // of the Save Passwords Settings.
  NSDate* successfulReauthTime_;
  // Module containing the reauthentication mechanism for viewing and copying
  // passwords.
  ReauthenticationModule* reauthenticationModule_;
}
// Kick off async request to get logins from password store.
- (void)getLoginsFromPasswordStore;
@end

@implementation SavePasswordsCollectionViewController

#pragma mark - Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    browserState_ = browserState;
    reauthenticationModule_ = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:self];
    self.title = l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS);
    self.shouldHideDoneButton = YES;
    passwordStore_ = IOSChromePasswordStoreFactory::GetForBrowserState(
        browserState_, ServiceAccessType::EXPLICIT_ACCESS);
    DCHECK(passwordStore_);
    passwordManagerEnabled_ = [[PrefBackedBoolean alloc]
        initWithPrefService:browserState_->GetPrefs()
                   prefName:password_manager::prefs::
                                kPasswordManagerSavingEnabled];
    [passwordManagerEnabled_ setObserver:self];
    [self getLoginsFromPasswordStore];
    [self updateEditButton];
    [self loadModel];
  }
  return self;
}

- (void)dealloc {
  [passwordManagerEnabled_ setObserver:nil];
}

#pragma mark - SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  // Manage account message.
  CollectionViewItem* manageAccountLinkItem = [self manageAccountLinkItem];
  if (manageAccountLinkItem) {
    [model addSectionWithIdentifier:SectionIdentifierMessage];
    [model addItem:manageAccountLinkItem
        toSectionWithIdentifier:SectionIdentifierMessage];
  }

  // Save passwords switch.
  [model addSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
  savePasswordsItem_ = [self savePasswordsItem];
  [model addItem:savePasswordsItem_
      toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];

  // Saved passwords.
  if ([passwordManagerEnabled_ value]) {
    if (!savedForms_.empty()) {
      [model addSectionWithIdentifier:SectionIdentifierSavedPasswords];
      CollectionViewTextItem* headerItem =
          [[CollectionViewTextItem alloc] initWithType:ItemTypeHeader];
      headerItem.text =
          l10n_util::GetNSString(IDS_PASSWORD_MANAGER_SHOW_PASSWORDS_TAB_TITLE);
      headerItem.textColor = [[MDCPalette greyPalette] tint500];
      [model setHeader:headerItem
          forSectionWithIdentifier:SectionIdentifierSavedPasswords];
      for (const auto& form : savedForms_) {
        [model addItem:[self savedFormItemWithForm:form.get()]
            toSectionWithIdentifier:SectionIdentifierSavedPasswords];
      }
    }
    if (!blacklistedForms_.empty()) {
      [model addSectionWithIdentifier:SectionIdentifierBlacklist];
      CollectionViewTextItem* headerItem =
          [[CollectionViewTextItem alloc] initWithType:ItemTypeHeader];
      headerItem.text =
          l10n_util::GetNSString(IDS_PASSWORD_MANAGER_EXCEPTIONS_TAB_TITLE);
      headerItem.textColor = [[MDCPalette greyPalette] tint500];
      [model setHeader:headerItem
          forSectionWithIdentifier:SectionIdentifierBlacklist];
      for (const auto& form : blacklistedForms_) {
        [model addItem:[self blacklistedFormItemWithForm:form.get()]
            toSectionWithIdentifier:SectionIdentifierBlacklist];
      }
    }
  }
}

#pragma mark - Items

- (CollectionViewItem*)manageAccountLinkItem {
  CollectionViewFooterItem* footerItem =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeManageAccount];
  footerItem.text =
      l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS_MANAGE_ACCOUNT);
  footerItem.linkURL = google_util::AppendGoogleLocaleParam(
      GURL(password_manager::kPasswordManagerAccountDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  footerItem.linkDelegate = self;
  return footerItem;
}

- (CollectionViewSwitchItem*)savePasswordsItem {
  CollectionViewSwitchItem* savePasswordsItem =
      [[CollectionViewSwitchItem alloc]
          initWithType:ItemTypeSavePasswordsSwitch];
  savePasswordsItem.text = l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS);
  savePasswordsItem.on = [passwordManagerEnabled_ value];
  return savePasswordsItem;
}

- (SavedFormContentItem*)savedFormItemWithForm:(autofill::PasswordForm*)form {
  SavedFormContentItem* passwordItem =
      [[SavedFormContentItem alloc] initWithType:ItemTypeSavedPassword];
  passwordItem.text =
      base::SysUTF8ToNSString(GetHumanReadableOriginCopy(*form));
  passwordItem.detailText = base::SysUTF16ToNSString(form->username_value);
  if (experimental_flags::IsViewCopyPasswordsEnabled()) {
    passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
    passwordItem.accessoryType =
        MDCCollectionViewCellAccessoryDisclosureIndicator;
  }
  return passwordItem;
}

- (BlacklistedFormContentItem*)blacklistedFormItemWithForm:
    (autofill::PasswordForm*)form {
  BlacklistedFormContentItem* passwordItem =
      [[BlacklistedFormContentItem alloc] initWithType:ItemTypeBlacklisted];
  passwordItem.text =
      base::SysUTF8ToNSString(GetHumanReadableOriginCopy(*form));
  return passwordItem;
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsEditing:(UICollectionView*)collectionView {
  return YES;
}

#pragma mark - MDCCollectionViewStylingDelegate

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];
  switch (sectionIdentifier) {
    case SectionIdentifierMessage:
      // Display the message in the default style with no "card" UI and no
      // section padding.
      return MDCCollectionViewCellStyleDefault;
    default:
      return self.styler.cellStyle;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  switch (sectionIdentifier) {
    case SectionIdentifierMessage:
      // Display the message without any background image or shadowing.
      return YES;
    default:
      return NO;
  }
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeManageAccount:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    case ItemTypeSavedPassword:
      return MDCCellDefaultTwoLineHeight;
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeSavePasswordsSwitch:
      return YES;
    default:
      return NO;
  }
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  if ([self.collectionViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeSavePasswordsSwitch) {
    CollectionViewSwitchCell* switchCell =
        base::mac::ObjCCastStrict<CollectionViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(savePasswordsSwitchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, passwordManagerEnabled_);

  // Update the item.
  savePasswordsItem_.on = [passwordManagerEnabled_ value];

  // Update the cell.
  [self reconfigureCellsForItems:@[ savePasswordsItem_ ]];

  // Update the rest of the UI.
  [self.editor setEditing:NO];
  [self updateEditButton];
  [self reloadData];
}

#pragma mark - Actions

- (void)savePasswordsSwitchChanged:(UISwitch*)switchView {
  // Update the setting.
  [passwordManagerEnabled_ setValue:switchView.on];

  // Update the item.
  savePasswordsItem_.on = [passwordManagerEnabled_ value];

  // Update the rest of the UI.
  [self.editor setEditing:NO];
  [self updateEditButton];
  [self reloadData];
}

#pragma mark - Private methods

- (void)getLoginsFromPasswordStore {
  savedPasswordsConsumer_.reset(
      new password_manager::SavePasswordsConsumer(self));
  passwordStore_->GetAutofillableLogins(savedPasswordsConsumer_.get());
  blacklistPasswordsConsumer_.reset(
      new password_manager::SavePasswordsConsumer(self));
  passwordStore_->GetBlacklistLogins(blacklistPasswordsConsumer_.get());
}

- (void)onGetPasswordStoreResults:
    (const std::vector<std::unique_ptr<autofill::PasswordForm>>&)result {
  for (auto it = result.begin(); it != result.end(); ++it) {
    // PasswordForm is needed when user wants to delete the site/password.
    auto form = base::MakeUnique<autofill::PasswordForm>(**it);
    if (form->blacklisted_by_user)
      blacklistedForms_.push_back(std::move(form));
    else
      savedForms_.push_back(std::move(form));
  }

  [self updateEditButton];
  [self reloadData];
}

- (BOOL)shouldShowEditButton {
  return [passwordManagerEnabled_ value];
}

- (BOOL)editButtonEnabled {
  DCHECK([self shouldShowEditButton]);
  return !savedForms_.empty() || !blacklistedForms_.empty();
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  // Actions should only take effect when not in editing mode.
  if (self.editing) {
    return;
  }

  CollectionViewModel* model = self.collectionViewModel;
  if ([model itemTypeForIndexPath:indexPath] == ItemTypeSavedPassword) {
    DCHECK_EQ([model sectionIdentifierForSection:indexPath.section],
              SectionIdentifierSavedPasswords);
    if (experimental_flags::IsViewCopyPasswordsEnabled()) {
      DCHECK_LT(base::checked_cast<size_t>(indexPath.item), savedForms_.size());
      autofill::PasswordForm* form = savedForms_[indexPath.item].get();
      NSString* username = base::SysUTF16ToNSString(form->username_value);
      NSString* password = base::SysUTF16ToNSString(form->password_value);
      NSString* origin =
          base::SysUTF8ToNSString(GetHumanReadableOriginCopy(*form));
      UIViewController* controller =
          [[PasswordDetailsCollectionViewController alloc]
                initWithPasswordForm:*form
                            delegate:self
              reauthenticationModule:reauthenticationModule_
                            username:username
                            password:password
                              origin:origin];
      [self.navigationController pushViewController:controller animated:YES];
    }
  }
}

#pragma mark MDCCollectionViewEditingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    canEditItemAtIndexPath:(NSIndexPath*)indexPath {
  // Only password cells are editable.
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  return [item isKindOfClass:[SavedFormContentItem class]] ||
         [item isKindOfClass:[BlacklistedFormContentItem class]];
}

- (void)collectionView:(UICollectionView*)collectionView
    willDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  // Ensure indexPaths are sorted to maintain delete logic, and keep track of
  // number of items deleted to adjust index for accessing elements in the
  // forms vectors.
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  int passwordsDeleted = 0;
  int blacklistedDeleted = 0;
  for (NSIndexPath* indexPath in sortedIndexPaths) {
    // Only form items are editable.
    CollectionViewTextItem* item =
        base::mac::ObjCCastStrict<CollectionViewTextItem>(
            [self.collectionViewModel itemAtIndexPath:indexPath]);
    BOOL blacklisted = [item isKindOfClass:[BlacklistedFormContentItem class]];
    unsigned int formIndex = (unsigned int)indexPath.item;
    // Adjust index to account for deleted items.
    formIndex -= blacklisted ? blacklistedDeleted : passwordsDeleted;
    auto& forms = blacklisted ? blacklistedForms_ : savedForms_;
    DCHECK_LT(formIndex, forms.size());
    auto formIterator = forms.begin() + formIndex;
    std::unique_ptr<autofill::PasswordForm> form = std::move(*formIterator);
    forms.erase(formIterator);
    passwordStore_->RemoveLogin(*form);
    deletedForms_.push_back(std::move(form));
    if (blacklisted) {
      ++blacklistedDeleted;
    } else {
      ++passwordsDeleted;
    }
  }

  // Must call super at the end of the child implementation.
  [super collectionView:collectionView willDeleteItemsAtIndexPaths:indexPaths];
}

- (void)collectionView:(UICollectionView*)collectionView
    didDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  // Remove empty sections.
  // TODO(crbug.com/593786): Move this logic in CollectionViewController.
  NSMutableOrderedSet* sectionsToRemove = [NSMutableOrderedSet orderedSet];
  // Sort and enumerate in reverse order to delete the items from the collection
  // view model.
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  for (NSIndexPath* indexPath in [sortedIndexPaths reverseObjectEnumerator]) {
    if ([collectionView numberOfItemsInSection:indexPath.section] == 0) {
      [sectionsToRemove addObject:@(indexPath.section)];
    }
  }
  __weak SavePasswordsCollectionViewController* weakSelf = self;
  [self.collectionView performBatchUpdates:^{
    SavePasswordsCollectionViewController* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    for (NSNumber* sectionNumber in sectionsToRemove) {
      NSInteger section = [sectionNumber integerValue];
      NSInteger sectionIdentifier = [[strongSelf collectionViewModel]
          sectionIdentifierForSection:section];
      [[strongSelf collectionViewModel]
          removeSectionWithIdentifier:sectionIdentifier];
      [[strongSelf collectionView]
          deleteSections:[NSIndexSet indexSetWithIndex:section]];
    }
  }
      completion:^(BOOL finished) {
        SavePasswordsCollectionViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        if (![strongSelf editButtonEnabled]) {
          [strongSelf setEditing:NO];
        }
        [strongSelf updateEditButton];
      }];
}

#pragma mark PasswordDetailsCollectionViewControllerDelegate

- (void)deletePassword:(const autofill::PasswordForm&)form {
  passwordStore_->RemoveLogin(form);
  for (auto it = savedForms_.begin(); it != savedForms_.end(); ++it) {
    if (**it == form) {
      savedForms_.erase(it);
      return;
    }
  }
  [self.navigationController popViewControllerAnimated:YES];
}

#pragma mark SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  successfulReauthTime_ = [[NSDate alloc] init];
}

- (NSDate*)lastSuccessfulReauthTime {
  return successfulReauthTime_;
}

@end
