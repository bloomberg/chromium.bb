// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"

#include <memory>

#import "base/ios/weak_nsobject.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/metrics/user_metrics.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/util.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_removal_controller.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/prefs/pref_observer_bridge.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_interaction_controller.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_account_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/settings/about_chrome_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/accounts_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/bandwidth_management_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/cells/account_signin_item.h"
#import "ios/chrome/browser/ui/settings/content_settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/material_cell_catalog_view_controller.h"
#import "ios/chrome/browser/ui/settings/native_apps_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/save_passwords_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/search_engine_settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/settings/voicesearch_collection_view_controller.h"
#import "ios/chrome/browser/ui/sync/sync_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/browser/voice/speech_input_locale_config.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_prefs.h"
#include "ui/base/l10n/l10n_util_mac.h"

NSString* const kSettingsCollectionViewId = @"kSettingsCollectionViewId";
NSString* const kSettingsSignInCellId = @"kSettingsSignInCellId";
NSString* const kSettingsAccountCellId = @"kSettingsAccountCellId";
NSString* const kSettingsSearchEngineCellId = @"Search Engine";
NSString* const kSettingsVoiceSearchCellId = @"Voice Search Settings";

@interface SettingsCollectionViewController (NotificationBridgeDelegate)
// Notifies this controller that the sign in state has changed.
- (void)onSignInStateChanged;
@end

namespace {

const CGFloat kAccountProfilePhotoDimension = 40.0f;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSignIn = kSectionIdentifierEnumZero,
  SectionIdentifierBasics,
  SectionIdentifierAdvanced,
  SectionIdentifierInfo,
  SectionIdentifierDebug,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSignInButton = kItemTypeEnumZero,
  ItemTypeAccount,
  ItemTypeHeader,
  ItemTypeSearchEngine,
  ItemTypeSavedPasswords,
  ItemTypeAutofill,
  ItemTypeNativeApps,
  ItemTypeVoiceSearch,
  ItemTypePrivacy,
  ItemTypeContentSettings,
  ItemTypeBandwidth,
  ItemTypeAboutChrome,
  ItemTypeMemoryDebugging,
  ItemTypeViewSource,
  ItemTypeLogJavascript,
  ItemTypeShowAutofillTypePredictions,
  ItemTypeCellCatalog,
};

#if CHROMIUM_BUILD && !defined(NDEBUG)
NSString* kDevViewSourceKey = @"DevViewSource";
NSString* kLogJavascriptKey = @"LogJavascript";
NSString* kShowAutofillTypePredictionsKey = @"ShowAutofillTypePredictions";
#endif  // CHROMIUM_BUILD && !defined(NDEBUG)

#pragma mark - SigninObserverBridge Class

class SigninObserverBridge : public SigninManagerBase::Observer {
 public:
  SigninObserverBridge(ios::ChromeBrowserState* browserState,
                       SettingsCollectionViewController* owner);
  ~SigninObserverBridge() override{};

  // SigninManagerBase::Observer implementation:
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username,
                             const std::string& password) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

 private:
  base::WeakNSObject<SettingsCollectionViewController> owner_;
  ScopedObserver<SigninManager, SigninObserverBridge> observer_;
};

SigninObserverBridge::SigninObserverBridge(
    ios::ChromeBrowserState* browserState,
    SettingsCollectionViewController* owner)
    : owner_(owner), observer_(this) {
  DCHECK(owner_);
  SigninManager* sigin_manager =
      ios::SigninManagerFactory::GetForBrowserState(browserState);
  if (!sigin_manager)
    return;
  observer_.Add(sigin_manager);
}

void SigninObserverBridge::GoogleSigninSucceeded(const std::string& account_id,
                                                 const std::string& username,
                                                 const std::string& password) {
  [owner_ onSignInStateChanged];
}

void SigninObserverBridge::GoogleSignedOut(const std::string& account_id,
                                           const std::string& username) {
  [owner_ onSignInStateChanged];
}

}  // namespace

#pragma mark - SettingsCollectionViewController

@interface SettingsCollectionViewController ()<SettingsControllerProtocol,
                                               SyncObserverModelBridge,
                                               ChromeIdentityServiceObserver,
                                               BooleanObserver,
                                               PrefObserverDelegate> {
  // The main browser state that hold the settings. Never off the record.
  ios::ChromeBrowserState* _mainBrowserState;  // weak

  // The current browser state. It is either |_mainBrowserState|
  // or |_mainBrowserState->GetOffTheRecordChromeBrowserState()|.
  ios::ChromeBrowserState* _currentBrowserState;  // weak
  std::unique_ptr<SigninObserverBridge> _notificationBridge;
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
  base::scoped_nsobject<SigninInteractionController>
      _signinInteractionController;
  // Whether the impression of the Signin button has already been recorded.
  BOOL _hasRecordedSigninImpression;
  // PrefBackedBoolean for ShowMemoryDebugTools switch.
  base::scoped_nsobject<PrefBackedBoolean> _showMemoryDebugToolsEnabled;
  // The item related to the switch for the show suggestions setting.
  base::scoped_nsobject<CollectionViewSwitchItem> _showMemoryDebugToolsItem;

  // Cached resized profile image.
  base::scoped_nsobject<UIImage> _resizedImage;
  base::WeakNSObject<UIImage> _oldImage;

  // Identity object and observer used for Account Item refresh.
  base::scoped_nsobject<ChromeIdentity> _identity;
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;

  // PrefMember for voice locale code.
  StringPrefMember _voiceLocaleCode;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // TODO(crbug.com/662435): Refactor PrefObserverBridge so it owns the
  // PrefChangeRegistrar.
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Updatable Items.
  base::scoped_nsobject<CollectionViewDetailItem> _voiceSearchDetailItem;
  base::scoped_nsobject<CollectionViewDetailItem> _defaultSearchEngineItem;
  base::scoped_nsobject<CollectionViewDetailItem> _savePasswordsDetailItem;
  base::scoped_nsobject<CollectionViewDetailItem> _autoFillDetailItem;
}

// Stops observing browser state services. This is required during the shutdown
// phase to avoid observing services for a profile that is being killed.
- (void)stopBrowserStateServiceObservers;

@end

@implementation SettingsCollectionViewController

#pragma mark Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)mainBrowserState
                 currentBrowserState:
                     (ios::ChromeBrowserState*)currentBrowserState {
  DCHECK(mainBrowserState);
  DCHECK(currentBrowserState);
  DCHECK_EQ(mainBrowserState,
            currentBrowserState->GetOriginalChromeBrowserState());
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    _mainBrowserState = mainBrowserState;
    _currentBrowserState = currentBrowserState;
    self.title = l10n_util::GetNSStringWithFixup(IDS_IOS_SETTINGS_TITLE);
    self.collectionViewAccessibilityIdentifier = kSettingsCollectionViewId;
    _notificationBridge.reset(
        new SigninObserverBridge(_mainBrowserState, self));
    syncer::SyncService* syncService =
        IOSChromeProfileSyncServiceFactory::GetForBrowserState(
            _mainBrowserState);
    _syncObserverBridge.reset(new SyncObserverBridge(self, syncService));

    _showMemoryDebugToolsEnabled.reset([[PrefBackedBoolean alloc]
        initWithPrefService:GetApplicationContext()->GetLocalState()
                   prefName:prefs::kShowMemoryDebuggingTools]);
    [_showMemoryDebugToolsEnabled setObserver:self];

    AuthenticationService* authService =
        AuthenticationServiceFactory::GetForBrowserState(_mainBrowserState);
    _identity.reset([authService->GetAuthenticatedIdentity() retain]);
    _identityServiceObserver.reset(
        new ChromeIdentityServiceObserverBridge(self));

    PrefService* prefService = _mainBrowserState->GetPrefs();

    _voiceLocaleCode.Init(prefs::kVoiceSearchLocale, prefService);

    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(prefs::kVoiceSearchLocale,
                                                     &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        password_manager::prefs::kPasswordManagerSavingEnabled,
        &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillEnabled, &_prefChangeRegistrar);

    [self loadModel];
  }
  return self;
}

- (void)dealloc {
  [self stopBrowserStateServiceObservers];
  [super dealloc];
}

- (void)stopBrowserStateServiceObservers {
  _syncObserverBridge.reset();
  _notificationBridge.reset();
  _identityServiceObserver.reset();
  [_showMemoryDebugToolsEnabled setObserver:nil];
}

- (SigninInteractionController*)signinInteractionController {
  return _signinInteractionController;
}

#pragma mark View lifecycle

// TODO(crbug.com/661915): Refactor TemplateURLObserver and re-implement this so
// it observes the default search engine name instead of reloading on
// ViewWillAppear.
- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateSearchCell];
}

#pragma mark SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];

  CollectionViewModel* model = self.collectionViewModel;

  // Sign in/Account section
  [model addSectionWithIdentifier:SectionIdentifierSignIn];
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(_mainBrowserState);
  if (!authService->IsAuthenticated()) {
    if (!_hasRecordedSigninImpression) {
      // Once the Settings are open, this button impression will at most be
      // recorded once until they are closed.
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromSettings"));
      _hasRecordedSigninImpression = YES;
    }
    [model addItem:[self signInTextItem]
        toSectionWithIdentifier:SectionIdentifierSignIn];
  } else {
    [model addItem:[self accountCellItem]
        toSectionWithIdentifier:SectionIdentifierSignIn];
  }

  // Basics section
  [model addSectionWithIdentifier:SectionIdentifierBasics];
  CollectionViewTextItem* basicsHeader = [
      [[CollectionViewTextItem alloc] initWithType:ItemTypeHeader] autorelease];
  basicsHeader.text = l10n_util::GetNSString(IDS_IOS_OPTIONS_GENERAL_TAB_LABEL);
  [model setHeader:basicsHeader
      forSectionWithIdentifier:SectionIdentifierBasics];
  [model addItem:[self searchEngineDetailItem]
      toSectionWithIdentifier:SectionIdentifierBasics];
  [model addItem:[self savePasswordsDetailItem]
      toSectionWithIdentifier:SectionIdentifierBasics];
  [model addItem:[self autoFillDetailItem]
      toSectionWithIdentifier:SectionIdentifierBasics];
  [model addItem:[self nativeAppsDetailItem]
      toSectionWithIdentifier:SectionIdentifierBasics];

  // Advanced Section
  [model addSectionWithIdentifier:SectionIdentifierAdvanced];
  CollectionViewTextItem* advancedHeader = [
      [[CollectionViewTextItem alloc] initWithType:ItemTypeHeader] autorelease];
  advancedHeader.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ADVANCED_TAB_LABEL);
  [model setHeader:advancedHeader
      forSectionWithIdentifier:SectionIdentifierAdvanced];
  [model addItem:[self voiceSearchDetailItem]
      toSectionWithIdentifier:SectionIdentifierAdvanced];
  [model addItem:[self privacyDetailItem]
      toSectionWithIdentifier:SectionIdentifierAdvanced];
  [model addItem:[self contentSettingsDetailItem]
      toSectionWithIdentifier:SectionIdentifierAdvanced];
  [model addItem:[self bandwidthManagementDetailItem]
      toSectionWithIdentifier:SectionIdentifierAdvanced];

  // Info Section
  [model addSectionWithIdentifier:SectionIdentifierInfo];
  [model addItem:[self aboutChromeDetailItem]
      toSectionWithIdentifier:SectionIdentifierInfo];

  // Debug Section
  if ([self hasDebugSection]) {
    [model addSectionWithIdentifier:SectionIdentifierDebug];
    CollectionViewTextItem* debugHeader = [[[CollectionViewTextItem alloc]
        initWithType:ItemTypeHeader] autorelease];
    debugHeader.text = @"Debug";
    [model setHeader:debugHeader
        forSectionWithIdentifier:SectionIdentifierDebug];
  }

  if (experimental_flags::IsMemoryDebuggingEnabled()) {
    _showMemoryDebugToolsItem.reset([[self showMemoryDebugSwitchItem] retain]);
    [model addItem:_showMemoryDebugToolsItem
        toSectionWithIdentifier:SectionIdentifierDebug];
  }

#if CHROMIUM_BUILD && !defined(NDEBUG)
  [model addItem:[self viewSourceSwitchItem]
      toSectionWithIdentifier:SectionIdentifierDebug];
  [model addItem:[self logJavascriptConsoleSwitchItem]
      toSectionWithIdentifier:SectionIdentifierDebug];
  [model addItem:[self showAutofillTypePredictionsSwitchItem]
      toSectionWithIdentifier:SectionIdentifierDebug];
  [model addItem:[self materialCatalogDetailItem]
      toSectionWithIdentifier:SectionIdentifierDebug];
#endif  // CHROMIUM_BUILD && !defined(NDEBUG)
}

#pragma mark - Model Items

- (CollectionViewItem*)signInTextItem {
  AccountSignInItem* signInTextItem = [[[AccountSignInItem alloc]
      initWithType:ItemTypeSignInButton] autorelease];
  signInTextItem.accessibilityIdentifier = kSettingsSignInCellId;
  UIImage* image = CircularImageFromImage(ios::GetChromeBrowserProvider()
                                              ->GetSigninResourcesProvider()
                                              ->GetDefaultAvatar(),
                                          kAccountProfilePhotoDimension);
  signInTextItem.image = image;

  return signInTextItem;
}

- (CollectionViewItem*)accountCellItem {
  CollectionViewAccountItem* identityAccountItem =
      [[[CollectionViewAccountItem alloc] initWithType:ItemTypeAccount]
          autorelease];
  identityAccountItem.accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  identityAccountItem.accessibilityIdentifier = kSettingsAccountCellId;
  [self updateIdentityAccountItem:identityAccountItem];
  return identityAccountItem;
}

- (CollectionViewItem*)searchEngineDetailItem {
  NSString* defaultSearchEngineName =
      base::SysUTF16ToNSString(GetDefaultSearchEngineName(
          ios::TemplateURLServiceFactory::GetForBrowserState(
              _mainBrowserState)));

  _defaultSearchEngineItem.reset(
      [[self detailItemWithType:ItemTypeSearchEngine
                           text:l10n_util::GetNSString(
                                    IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)
                     detailText:defaultSearchEngineName] retain]);
  _defaultSearchEngineItem.get().accessibilityIdentifier =
      kSettingsSearchEngineCellId;
  return _defaultSearchEngineItem;
}

- (CollectionViewItem*)savePasswordsDetailItem {
  BOOL savePasswordsEnabled = _mainBrowserState->GetPrefs()->GetBoolean(
      password_manager::prefs::kPasswordManagerSavingEnabled);
  NSString* passwordsDetail = savePasswordsEnabled
                                  ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                  : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);

  _savePasswordsDetailItem.reset(
      [[self detailItemWithType:ItemTypeSavedPasswords
                           text:l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS)
                     detailText:passwordsDetail] retain]);

  return _savePasswordsDetailItem;
}

- (CollectionViewItem*)autoFillDetailItem {
  BOOL autofillEnabled = _mainBrowserState->GetPrefs()->GetBoolean(
      autofill::prefs::kAutofillEnabled);
  NSString* autofillDetail = autofillEnabled
                                 ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                 : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _autoFillDetailItem.reset(
      [[self detailItemWithType:ItemTypeAutofill
                           text:l10n_util::GetNSString(IDS_IOS_AUTOFILL)
                     detailText:autofillDetail] retain]);

  return _autoFillDetailItem;
}

- (CollectionViewItem*)nativeAppsDetailItem {
  return [self
      detailItemWithType:ItemTypeNativeApps
                    text:l10n_util::GetNSString(IDS_IOS_GOOGLE_APPS_SM_SETTINGS)
              detailText:nil];
}

- (CollectionViewItem*)voiceSearchDetailItem {
  voice::SpeechInputLocaleConfig* localeConfig =
      voice::SpeechInputLocaleConfig::GetInstance();
  voice::SpeechInputLocale locale =
      _voiceLocaleCode.GetValue().length()
          ? localeConfig->GetLocaleForCode(_voiceLocaleCode.GetValue())
          : localeConfig->GetDefaultLocale();
  NSString* languageName = base::SysUTF16ToNSString(locale.display_name);
  _voiceSearchDetailItem.reset(
      [[self detailItemWithType:ItemTypeVoiceSearch
                           text:l10n_util::GetNSString(
                                    IDS_IOS_VOICE_SEARCH_SETTING_TITLE)
                     detailText:languageName] retain]);
  _voiceSearchDetailItem.get().accessibilityIdentifier =
      kSettingsVoiceSearchCellId;
  return _voiceSearchDetailItem;
}

- (CollectionViewItem*)privacyDetailItem {
  return
      [self detailItemWithType:ItemTypePrivacy
                          text:l10n_util::GetNSString(
                                   IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY)
                    detailText:nil];
}

- (CollectionViewItem*)contentSettingsDetailItem {
  return [self
      detailItemWithType:ItemTypeContentSettings
                    text:l10n_util::GetNSString(IDS_IOS_CONTENT_SETTINGS_TITLE)
              detailText:nil];
}

- (CollectionViewItem*)bandwidthManagementDetailItem {
  return [self detailItemWithType:ItemTypeBandwidth
                             text:l10n_util::GetNSString(
                                      IDS_IOS_BANDWIDTH_MANAGEMENT_SETTINGS)
                       detailText:nil];
}

- (CollectionViewItem*)aboutChromeDetailItem {
  return [self detailItemWithType:ItemTypeAboutChrome
                             text:l10n_util::GetNSString(IDS_IOS_PRODUCT_NAME)
                       detailText:nil];
}

- (CollectionViewSwitchItem*)showMemoryDebugSwitchItem {
  CollectionViewSwitchItem* showMemoryDebugSwitchItem =
      [self switchItemWithType:ItemTypeMemoryDebugging
                         title:@"Show memory debug tools"
               withDefaultsKey:nil];
  showMemoryDebugSwitchItem.on = [_showMemoryDebugToolsEnabled value];

  return showMemoryDebugSwitchItem;
}
#if CHROMIUM_BUILD && !defined(NDEBUG)

- (CollectionViewSwitchItem*)viewSourceSwitchItem {
  return [self switchItemWithType:ItemTypeViewSource
                            title:@"View source menu"
                  withDefaultsKey:kDevViewSourceKey];
}

- (CollectionViewSwitchItem*)logJavascriptConsoleSwitchItem {
  return [self switchItemWithType:ItemTypeLogJavascript
                            title:@"Log JS"
                  withDefaultsKey:kLogJavascriptKey];
}

- (CollectionViewSwitchItem*)showAutofillTypePredictionsSwitchItem {
  return [self switchItemWithType:ItemTypeShowAutofillTypePredictions
                            title:@"Show Autofill type predictions"
                  withDefaultsKey:kShowAutofillTypePredictionsKey];
}

- (CollectionViewDetailItem*)materialCatalogDetailItem {
  return [self detailItemWithType:ItemTypeCellCatalog
                             text:@"Cell Catalog"
                       detailText:nil];
}
#endif  // CHROMIUM_BUILD && !defined(NDEBUG)

#pragma mark Item Updaters

- (void)updateSearchCell {
  NSString* defaultSearchEngineName =
      base::SysUTF16ToNSString(GetDefaultSearchEngineName(
          ios::TemplateURLServiceFactory::GetForBrowserState(
              _mainBrowserState)));

  _defaultSearchEngineItem.get().detailText = defaultSearchEngineName;
  [self reconfigureCellsForItems:@[ _defaultSearchEngineItem ]
         inSectionWithIdentifier:SectionIdentifierBasics];
}

#pragma mark Item Constructors

- (CollectionViewDetailItem*)detailItemWithType:(NSInteger)type
                                           text:(NSString*)text
                                     detailText:(NSString*)detailText {
  CollectionViewDetailItem* detailItem =
      [[[CollectionViewDetailItem alloc] initWithType:type] autorelease];
  detailItem.text = text;
  detailItem.detailText = detailText;
  detailItem.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;

  return detailItem;
}

- (CollectionViewSwitchItem*)switchItemWithType:(NSInteger)type
                                          title:(NSString*)title
                                withDefaultsKey:(NSString*)key {
  CollectionViewSwitchItem* switchItem =
      [[[CollectionViewSwitchItem alloc] initWithType:type] autorelease];
  switchItem.text = title;
  if (key) {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    switchItem.on = [defaults boolForKey:key];
  }

  return switchItem;
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];

  if ([cell isKindOfClass:[CollectionViewDetailCell class]]) {
    CollectionViewDetailCell* detailCell =
        base::mac::ObjCCastStrict<CollectionViewDetailCell>(cell);
    if (itemType == ItemTypeSavedPasswords) {
      scoped_refptr<password_manager::PasswordStore> passwordStore =
          IOSChromePasswordStoreFactory::GetForBrowserState(
              _mainBrowserState, ServiceAccessType::EXPLICIT_ACCESS);
      if (!passwordStore) {
        // The password store factory returns a NULL password store if something
        // goes wrong during the password store initialization. Disable the save
        // passwords cell in this case.
        LOG(ERROR) << "Save passwords cell was disabled as the password store"
                      " cannot be created.";
        [detailCell setUserInteractionEnabled:NO];
        detailCell.textLabel.textColor = [[MDCPalette greyPalette] tint500];
        detailCell.detailTextLabel.textColor =
            [[MDCPalette greyPalette] tint400];
      }
    } else {
      [detailCell setUserInteractionEnabled:YES];
      detailCell.textLabel.textColor = [[MDCPalette greyPalette] tint900];
      detailCell.detailTextLabel.textColor = [[MDCPalette greyPalette] tint500];
    }
  }

  switch (itemType) {
    case ItemTypeMemoryDebugging: {
      CollectionViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<CollectionViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(memorySwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeViewSource: {
#if CHROMIUM_BUILD && !defined(NDEBUG)
      CollectionViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<CollectionViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(viewSourceSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
#else
      NOTREACHED();
#endif  // CHROMIUM_BUILD && !defined(NDEBUG)
      break;
    }
    case ItemTypeLogJavascript: {
#if CHROMIUM_BUILD && !defined(NDEBUG)
      CollectionViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<CollectionViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(logJSSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
#else
      NOTREACHED();
#endif  // CHROMIUM_BUILD && !defined(NDEBUG)
      break;
    }
    case ItemTypeShowAutofillTypePredictions: {
#if CHROMIUM_BUILD && !defined(NDEBUG)
      CollectionViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<CollectionViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(showAutoFillSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
#else
      NOTREACHED();
#endif  // CHROMIUM_BUILD && !defined(NDEBUG)
      break;
    }
    default:
      break;
  }

  return cell;
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  UICollectionReusableView* view = [super collectionView:collectionView
                       viewForSupplementaryElementOfKind:kind
                                             atIndexPath:indexPath];

  MDCCollectionViewTextCell* textCell =
      base::mac::ObjCCast<MDCCollectionViewTextCell>(view);
  if (textCell) {
    textCell.textLabel.textColor = [[MDCPalette greyPalette] tint500];
  }
  return view;
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  id object = [self.collectionViewModel itemAtIndexPath:indexPath];
  if ([object respondsToSelector:@selector(isEnabled)] &&
      ![object performSelector:@selector(isEnabled)]) {
    // Don't perform any action if the cell isn't enabled.
    return;
  }

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];

  base::scoped_nsobject<UIViewController> controller;

  switch (itemType) {
    case ItemTypeSignInButton:
      [self showSignIn];
      break;
    case ItemTypeAccount:
      controller.reset([[AccountsCollectionViewController alloc]
               initWithBrowserState:_mainBrowserState
          closeSettingsOnAddAccount:NO]);
      break;
    case ItemTypeSearchEngine:
      controller.reset([[SearchEngineSettingsCollectionViewController alloc]
          initWithBrowserState:_mainBrowserState]);
      break;
    case ItemTypeSavedPasswords: {
      controller.reset([[SavePasswordsCollectionViewController alloc]
          initWithBrowserState:_mainBrowserState]);
      break;
    }
    case ItemTypeAutofill:
      controller.reset([[AutofillCollectionViewController alloc]
          initWithBrowserState:_mainBrowserState]);
      break;
    case ItemTypeNativeApps:
      controller.reset([[NativeAppsCollectionViewController alloc]
          initWithURLRequestContextGetter:_currentBrowserState
                                              ->GetRequestContext()]);
      break;
    case ItemTypeVoiceSearch:
      controller.reset([[VoicesearchCollectionViewController alloc]
          initWithPrefs:_mainBrowserState->GetPrefs()]);
      break;
    case ItemTypePrivacy:
      controller.reset([[PrivacyCollectionViewController alloc]
          initWithBrowserState:_mainBrowserState]);
      break;
    case ItemTypeContentSettings:
      controller.reset([[ContentSettingsCollectionViewController alloc]
          initWithBrowserState:_mainBrowserState]);
      break;
    case ItemTypeBandwidth:
      controller.reset([[BandwidthManagementCollectionViewController alloc]
          initWithBrowserState:_mainBrowserState]);
      break;
    case ItemTypeAboutChrome:
      controller.reset([[AboutChromeCollectionViewController alloc] init]);
      break;
    case ItemTypeMemoryDebugging:
    case ItemTypeViewSource:
    case ItemTypeLogJavascript:
    case ItemTypeShowAutofillTypePredictions:
      // Taps on these don't do anything. They have a switch as accessory view
      // and only the switch is tappable.
      break;
    case ItemTypeCellCatalog:
      controller.reset([[MaterialCellCatalogViewController alloc] init]);
      break;
    default:
      break;
  }

  if (controller) {
    [self.navigationController pushViewController:controller animated:YES];
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

  if (item.type == ItemTypeAccount) {
    return MDCCellDefaultTwoLineHeight;
  }

  if (item.type == ItemTypeSignInButton) {
    return MDCCellDefaultThreeLineHeight;
  }

  return MDCCellDefaultOneLineHeight;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeMemoryDebugging:
    case ItemTypeViewSource:
    case ItemTypeLogJavascript:
    case ItemTypeShowAutofillTypePredictions:
      return YES;
    default:
      return NO;
  }
}

#pragma mark Switch Actions

- (void)memorySwitchToggled:(UISwitch*)sender {
  NSIndexPath* switchPath =
      [self.collectionViewModel indexPathForItemType:ItemTypeMemoryDebugging
                                   sectionIdentifier:SectionIdentifierDebug];

  CollectionViewSwitchItem* switchItem =
      base::mac::ObjCCastStrict<CollectionViewSwitchItem>(
          [self.collectionViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [_showMemoryDebugToolsEnabled setValue:newSwitchValue];
}

#if CHROMIUM_BUILD && !defined(NDEBUG)
- (void)viewSourceSwitchToggled:(UISwitch*)sender {
  NSIndexPath* switchPath =
      [self.collectionViewModel indexPathForItemType:ItemTypeViewSource
                                   sectionIdentifier:SectionIdentifierDebug];

  CollectionViewSwitchItem* switchItem =
      base::mac::ObjCCastStrict<CollectionViewSwitchItem>(
          [self.collectionViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [self setBooleanNSUserDefaultsValue:newSwitchValue forKey:kDevViewSourceKey];
}

- (void)logJSSwitchToggled:(UISwitch*)sender {
  NSIndexPath* switchPath =
      [self.collectionViewModel indexPathForItemType:ItemTypeLogJavascript
                                   sectionIdentifier:SectionIdentifierDebug];

  CollectionViewSwitchItem* switchItem =
      base::mac::ObjCCastStrict<CollectionViewSwitchItem>(
          [self.collectionViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [self setBooleanNSUserDefaultsValue:newSwitchValue forKey:kLogJavascriptKey];
}

- (void)showAutoFillSwitchToggled:(UISwitch*)sender {
  NSIndexPath* switchPath = [self.collectionViewModel
      indexPathForItemType:ItemTypeShowAutofillTypePredictions
         sectionIdentifier:SectionIdentifierDebug];

  CollectionViewSwitchItem* switchItem =
      base::mac::ObjCCastStrict<CollectionViewSwitchItem>(
          [self.collectionViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [self setBooleanNSUserDefaultsValue:newSwitchValue
                               forKey:kShowAutofillTypePredictionsKey];
}
#endif  // CHROMIUM_BUILD && !defined(NDEBUG)

#pragma mark Private methods

// Sets the NSUserDefaults BOOL |value| for |key|.
- (void)setBooleanNSUserDefaultsValue:(BOOL)value forKey:(NSString*)key {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:value forKey:key];
  [defaults synchronize];
}

// Returns YES if a "Debug" section should be shown. This is always true for
// Chromium builds, but for official builds it is gated by an experimental flag
// because the "Debug" section should never be showing in stable channel.
- (BOOL)hasDebugSection {
#if CHROMIUM_BUILD && !defined(NDEBUG)
  return YES;
#else
  if (experimental_flags::IsMemoryDebuggingEnabled()) {
    return YES;
  }
  return NO;
#endif  // CHROMIUM_BUILD && !defined(NDEBUG)
}

// Updates the identity cell.
- (void)updateIdentityAccountItem:
    (CollectionViewAccountItem*)identityAccountItem {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(_mainBrowserState);
  _identity.reset([authService->GetAuthenticatedIdentity() retain]);
  if (!_identity) {
    // This could occur during the sign out process. Just ignore as the account
    // cell will be replaced by the "Sign in" button.
    return;
  }
  identityAccountItem.image = [self userAccountImage];
  identityAccountItem.text = [_identity userFullName];

  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(_mainBrowserState);
  if (!syncSetupService->HasFinishedInitialSetup()) {
    identityAccountItem.detailText =
        l10n_util::GetNSString(IDS_IOS_SYNC_SETUP_IN_PROGRESS);
    identityAccountItem.shouldDisplayError = NO;
    return;
  }
  identityAccountItem.shouldDisplayError =
      !ios_internal::sync::IsTransientSyncError(
          syncSetupService->GetSyncServiceState());
  if (identityAccountItem.shouldDisplayError) {
    identityAccountItem.detailText =
        ios_internal::sync::GetSyncErrorDescriptionForBrowserState(
            _mainBrowserState);
  } else {
    identityAccountItem.detailText =
        syncSetupService->IsSyncEnabled()
            ? l10n_util::GetNSStringF(
                  IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SYNCING,
                  base::SysNSStringToUTF16([_identity userEmail]))
            : l10n_util::GetNSString(
                  IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SYNC_OFF);
  }
}

- (void)reloadAccountCell {
  if (![self.collectionViewModel hasItemForItemType:ItemTypeAccount
                                  sectionIdentifier:SectionIdentifierSignIn]) {
    return;
  }
  NSIndexPath* accountCellIndexPath =
      [self.collectionViewModel indexPathForItemType:ItemTypeAccount
                                   sectionIdentifier:SectionIdentifierSignIn];
  CollectionViewAccountItem* identityAccountItem =
      base::mac::ObjCCast<CollectionViewAccountItem>(
          [self.collectionViewModel itemAtIndexPath:accountCellIndexPath]);
  if (identityAccountItem) {
    [self updateIdentityAccountItem:identityAccountItem];
    [self reconfigureCellsForItems:@[ identityAccountItem ]
           inSectionWithIdentifier:SectionIdentifierSignIn];
  }
}

#pragma mark Sign in

- (void)showSignIn {
  base::RecordAction(base::UserMetricsAction("Signin_Signin_FromSettings"));
  DCHECK(!_signinInteractionController);
  _signinInteractionController.reset([[SigninInteractionController alloc]
          initWithBrowserState:_mainBrowserState
      presentingViewController:self.navigationController
         isPresentedOnSettings:YES
             signInAccessPoint:signin_metrics::AccessPoint::
                                   ACCESS_POINT_SETTINGS]);

  base::WeakNSObject<SettingsCollectionViewController> weakSelf(self);
  [_signinInteractionController signInWithCompletion:^(BOOL success) {
    [weakSelf didFinishSignin:success];
  }
                                      viewController:self];
}

- (void)didFinishSignin:(BOOL)signedIn {
  _signinInteractionController.reset();
}

#pragma mark NotificationBridgeDelegate

- (void)onSignInStateChanged {
  // Sign in state changes are rare. Just reload the entire collection when this
  // happens.
  [self reloadData];
}

#pragma mark SettingsControllerProtocol

- (void)settingsWillBeDismissed {
  [_signinInteractionController cancel];
  [self stopBrowserStateServiceObservers];
}

#pragma mark SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self reloadAccountCell];
}

#pragma mark - IdentityRefreshLogic

// Image used for loggedin user account that supports caching.
- (UIImage*)userAccountImage {
  UIImage* image = ios::GetChromeBrowserProvider()
                       ->GetChromeIdentityService()
                       ->GetCachedAvatarForIdentity(_identity);
  if (!image) {
    image = ios::GetChromeBrowserProvider()
                ->GetSigninResourcesProvider()
                ->GetDefaultAvatar();
    // No cached image, trigger a fetch, which will notify all observers
    // (including the corresponding AccountViewBase).
    ios::GetChromeBrowserProvider()
        ->GetChromeIdentityService()
        ->GetAvatarForIdentity(_identity, ^(UIImage*){
                               });
  }

  // If the currently used image has already been resized, use it.
  if (_resizedImage && _oldImage.get() == image)
    return _resizedImage;

  _oldImage.reset(image);

  // Resize the profile image.
  CGFloat dimension = kAccountProfilePhotoDimension;
  if (image.size.width != dimension || image.size.height != dimension) {
    image = ResizeImage(image, CGSizeMake(dimension, dimension),
                        ProjectionMode::kAspectFit);
  }
  _resizedImage.reset([image retain]);
  return _resizedImage;
}

#pragma mark ChromeIdentityServiceObserver

- (void)onProfileUpdate:(ChromeIdentity*)identity {
  if (identity == _identity) {
    [self reloadAccountCell];
  }
}

- (void)onChromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, _showMemoryDebugToolsEnabled.get());
  // Update the Item.
  _showMemoryDebugToolsItem.get().on = [_showMemoryDebugToolsEnabled value];

  // Update the Cell.
  [self reconfigureCellsForItems:@[ _showMemoryDebugToolsItem ]
         inSectionWithIdentifier:SectionIdentifierDebug];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kVoiceSearchLocale) {
    voice::SpeechInputLocaleConfig* localeConfig =
        voice::SpeechInputLocaleConfig::GetInstance();
    voice::SpeechInputLocale locale =
        _voiceLocaleCode.GetValue().length()
            ? localeConfig->GetLocaleForCode(_voiceLocaleCode.GetValue())
            : localeConfig->GetDefaultLocale();
    NSString* languageName = base::SysUTF16ToNSString(locale.display_name);
    _voiceSearchDetailItem.get().detailText = languageName;
    [self reconfigureCellsForItems:@[ _voiceSearchDetailItem ]
           inSectionWithIdentifier:SectionIdentifierAdvanced];
  }

  if (preferenceName ==
      password_manager::prefs::kPasswordManagerSavingEnabled) {
    BOOL savePasswordsEnabled =
        _mainBrowserState->GetPrefs()->GetBoolean(preferenceName);
    NSString* passwordsDetail =
        savePasswordsEnabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                             : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);

    _savePasswordsDetailItem.get().detailText = passwordsDetail;
    [self reconfigureCellsForItems:@[ _savePasswordsDetailItem ]
           inSectionWithIdentifier:SectionIdentifierBasics];
  }

  if (preferenceName == autofill::prefs::kAutofillEnabled) {
    BOOL autofillEnabled =
        _mainBrowserState->GetPrefs()->GetBoolean(preferenceName);
    NSString* autofillDetail =
        autofillEnabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                        : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _autoFillDetailItem.get().detailText = autofillDetail;
    [self reconfigureCellsForItems:@[ _autoFillDetailItem ]
           inSectionWithIdentifier:SectionIdentifierBasics];
  }
}

@end
