// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_setup_handler.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/sync/signin_histogram.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/signin/signin_manager_base.h"
#else
#include "chrome/browser/signin/signin_manager.h"
#endif

using content::WebContents;
using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;

namespace {

// A structure which contains all the configuration information for sync.
struct SyncConfigInfo {
  SyncConfigInfo();
  ~SyncConfigInfo();

  bool encrypt_all;
  bool sync_everything;
  bool sync_nothing;
  syncer::ModelTypeSet data_types;
  std::string passphrase;
  bool passphrase_is_gaia;
};

SyncConfigInfo::SyncConfigInfo()
    : encrypt_all(false),
      sync_everything(false),
      sync_nothing(false),
      passphrase_is_gaia(false) {
}

SyncConfigInfo::~SyncConfigInfo() {}

// Note: The order of these types must match the ordering of
// the respective types in ModelType
const char* kDataTypeNames[] = {
  "bookmarks",
  "preferences",
  "passwords",
  "autofill",
  "themes",
  "typedUrls",
  "extensions",
  "apps",
  "tabs"
};

COMPILE_ASSERT(28 == syncer::MODEL_TYPE_COUNT,
               update_kDataTypeNames_to_match_UserSelectableTypes);

typedef std::map<syncer::ModelType, const char*> ModelTypeNameMap;

ModelTypeNameMap GetSelectableTypeNameMap() {
  ModelTypeNameMap type_names;
  syncer::ModelTypeSet type_set = syncer::UserSelectableTypes();
  syncer::ModelTypeSet::Iterator it = type_set.First();
  DCHECK_EQ(arraysize(kDataTypeNames), type_set.Size());
  for (size_t i = 0; i < arraysize(kDataTypeNames) && it.Good();
       ++i, it.Inc()) {
    type_names[it.Get()] = kDataTypeNames[i];
  }
  return type_names;
}

#if !defined(OS_CHROMEOS)
// Signin logic not needed on ChromeOS
void BringTabToFront(WebContents* web_contents) {
  DCHECK(web_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    if (tab_strip_model) {
      int index = tab_strip_model->GetIndexOfWebContents(web_contents);
      if (index != TabStripModel::kNoTab)
        tab_strip_model->ActivateTabAt(index, false);
    }
  }
}

void CloseTab(content::WebContents* tab) {
  Browser* browser = chrome::FindBrowserWithWebContents(tab);
  if (browser) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    if (tab_strip_model) {
      int index = tab_strip_model->GetIndexOfWebContents(tab);
      if (index != TabStripModel::kNoTab) {
        tab_strip_model->ExecuteContextMenuCommand(
            index, TabStripModel::CommandCloseTab);
      }
    }
  }
}

#endif

bool GetConfiguration(const std::string& json, SyncConfigInfo* config) {
  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json));
  DictionaryValue* result;
  if (!parsed_value || !parsed_value->GetAsDictionary(&result)) {
    DLOG(ERROR) << "GetConfiguration() not passed a Dictionary";
    return false;
  }

  if (!result->GetBoolean("syncAllDataTypes", &config->sync_everything)) {
    DLOG(ERROR) << "GetConfiguration() not passed a syncAllDataTypes value";
    return false;
  }

  if (!result->GetBoolean("syncNothing", &config->sync_nothing)) {
    DLOG(ERROR) << "GetConfiguration() not passed a syncNothing value";
    return false;
  }

  DCHECK(!(config->sync_everything && config->sync_nothing))
      << "syncAllDataTypes and syncNothing cannot both be true";

  ModelTypeNameMap type_names = GetSelectableTypeNameMap();

  for (ModelTypeNameMap::const_iterator it = type_names.begin();
       it != type_names.end(); ++it) {
    std::string key_name = it->second + std::string("Synced");
    bool sync_value;
    if (!result->GetBoolean(key_name, &sync_value)) {
      DLOG(ERROR) << "GetConfiguration() not passed a value for " << key_name;
      return false;
    }
    if (sync_value)
      config->data_types.Put(it->first);
  }

  // Encryption settings.
  if (!result->GetBoolean("encryptAllData", &config->encrypt_all)) {
    DLOG(ERROR) << "GetConfiguration() not passed a value for encryptAllData";
    return false;
  }

  // Passphrase settings.
  bool have_passphrase;
  if (!result->GetBoolean("usePassphrase", &have_passphrase)) {
    DLOG(ERROR) << "GetConfiguration() not passed a usePassphrase value";
    return false;
  }

  if (have_passphrase) {
    if (!result->GetBoolean("isGooglePassphrase",
                            &config->passphrase_is_gaia)) {
      DLOG(ERROR) << "GetConfiguration() not passed isGooglePassphrase value";
      return false;
    }
    if (!result->GetString("passphrase", &config->passphrase)) {
      DLOG(ERROR) << "GetConfiguration() not passed a passphrase value";
      return false;
    }
  }
  return true;
}

bool IsKeystoreEncryptionEnabled() {
  return true;
}

}  // namespace

SyncSetupHandler::SyncSetupHandler(ProfileManager* profile_manager)
    : configuring_sync_(false),
#if !defined(OS_CHROMEOS)
      last_signin_error_(GoogleServiceAuthError::NONE),
      retry_on_signin_failure_(true),
      active_gaia_signin_tab_(NULL),
#endif
      profile_manager_(profile_manager) {
}

SyncSetupHandler::~SyncSetupHandler() {
  // Just exit if running unit tests (no actual WebUI is attached).
  if (!web_ui())
    return;

  // This case is hit when the user performs a back navigation.
  CloseSyncSetup();
}

void SyncSetupHandler::GetLocalizedValues(DictionaryValue* localized_strings) {
  GetStaticLocalizedValues(localized_strings, web_ui());
}

void SyncSetupHandler::GetStaticLocalizedValues(
    DictionaryValue* localized_strings,
    content::WebUI* web_ui) {
  DCHECK(localized_strings);

  string16 product_name(GetStringUTF16(IDS_PRODUCT_NAME));
  localized_strings->SetString(
      "chooseDataTypesInstructions",
      GetStringFUTF16(IDS_SYNC_CHOOSE_DATATYPES_INSTRUCTIONS, product_name));
  localized_strings->SetString(
      "encryptionInstructions",
      GetStringFUTF16(IDS_SYNC_ENCRYPTION_INSTRUCTIONS, product_name));
  localized_strings->SetString(
      "encryptionHelpURL", chrome::kSyncEncryptionHelpURL);
  localized_strings->SetString(
      "passphraseEncryptionMessage",
      GetStringFUTF16(IDS_SYNC_PASSPHRASE_ENCRYPTION_MESSAGE, product_name));
  localized_strings->SetString(
      "encryptionSectionMessage",
      GetStringFUTF16(IDS_SYNC_ENCRYPTION_SECTION_MESSAGE, product_name));
  localized_strings->SetString(
      "passphraseRecover",
      GetStringFUTF16(IDS_SYNC_PASSPHRASE_RECOVER,
                      ASCIIToUTF16(google_util::StringAppendGoogleLocaleParam(
                          chrome::kSyncGoogleDashboardURL))));
  localized_strings->SetString("stopSyncingExplanation",
      l10n_util::GetStringFUTF16(
          IDS_SYNC_STOP_SYNCING_EXPLANATION_LABEL,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          ASCIIToUTF16(google_util::StringAppendGoogleLocaleParam(
              chrome::kSyncGoogleDashboardURL))));
  localized_strings->SetString("stopSyncingTitle",
      l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_DIALOG_TITLE));
  localized_strings->SetString("stopSyncingConfirm",
        l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_CONFIRM_BUTTON_LABEL));

  localized_strings->SetString(
      "syncEverythingHelpURL", chrome::kSyncEverythingLearnMoreURL);
  localized_strings->SetString(
      "syncErrorHelpURL", chrome::kSyncErrorsHelpURL);

  static OptionsStringResource resources[] = {
    { "syncSetupConfigureTitle", IDS_SYNC_SETUP_CONFIGURE_TITLE },
    { "syncSetupSpinnerTitle", IDS_SYNC_SETUP_SPINNER_TITLE },
    { "syncSetupTimeoutTitle", IDS_SYNC_SETUP_TIME_OUT_TITLE },
    { "syncSetupTimeoutContent", IDS_SYNC_SETUP_TIME_OUT_CONTENT },
    { "errorLearnMore", IDS_LEARN_MORE },
    { "cancel", IDS_CANCEL },
    { "loginSuccess", IDS_SYNC_SUCCESS },
    { "settingUp", IDS_SYNC_LOGIN_SETTING_UP },
    { "syncAllDataTypes", IDS_SYNC_EVERYTHING },
    { "chooseDataTypes", IDS_SYNC_CHOOSE_DATATYPES },
    { "syncNothing", IDS_SYNC_NOTHING },
    { "bookmarks", IDS_SYNC_DATATYPE_BOOKMARKS },
    { "preferences", IDS_SYNC_DATATYPE_PREFERENCES },
    { "autofill", IDS_SYNC_DATATYPE_AUTOFILL },
    { "themes", IDS_SYNC_DATATYPE_THEMES },
    { "passwords", IDS_SYNC_DATATYPE_PASSWORDS },
    { "extensions", IDS_SYNC_DATATYPE_EXTENSIONS },
    { "typedURLs", IDS_SYNC_DATATYPE_TYPED_URLS },
    { "apps", IDS_SYNC_DATATYPE_APPS },
    { "openTabs", IDS_SYNC_DATATYPE_TABS },
    { "syncZeroDataTypesError", IDS_SYNC_ZERO_DATA_TYPES_ERROR },
    { "serviceUnavailableError", IDS_SYNC_SETUP_ABORTED_BY_PENDING_CLEAR },
    { "googleOption", IDS_SYNC_PASSPHRASE_OPT_GOOGLE },
    { "explicitOption", IDS_SYNC_PASSPHRASE_OPT_EXPLICIT },
    { "sectionGoogleMessage", IDS_SYNC_PASSPHRASE_MSG_GOOGLE },
    { "sectionExplicitMessage", IDS_SYNC_PASSPHRASE_MSG_EXPLICIT },
    { "passphraseLabel", IDS_SYNC_PASSPHRASE_LABEL },
    { "confirmLabel", IDS_SYNC_CONFIRM_PASSPHRASE_LABEL },
    { "emptyErrorMessage", IDS_SYNC_EMPTY_PASSPHRASE_ERROR },
    { "mismatchErrorMessage", IDS_SYNC_PASSPHRASE_MISMATCH_ERROR },
    { "passphraseWarning", IDS_SYNC_PASSPHRASE_WARNING },
    { "customizeLinkLabel", IDS_SYNC_CUSTOMIZE_LINK_LABEL },
    { "confirmSyncPreferences", IDS_SYNC_CONFIRM_SYNC_PREFERENCES },
    { "syncEverything", IDS_SYNC_SYNC_EVERYTHING },
    { "useDefaultSettings", IDS_SYNC_USE_DEFAULT_SETTINGS },
    { "passphraseSectionTitle", IDS_SYNC_PASSPHRASE_SECTION_TITLE },
    { "enterPassphraseTitle", IDS_SYNC_ENTER_PASSPHRASE_TITLE },
    { "enterPassphraseBody", IDS_SYNC_ENTER_PASSPHRASE_BODY },
    { "enterGooglePassphraseBody", IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY },
    { "passphraseLabel", IDS_SYNC_PASSPHRASE_LABEL },
    { "incorrectPassphrase", IDS_SYNC_INCORRECT_PASSPHRASE },
    { "passphraseWarning", IDS_SYNC_PASSPHRASE_WARNING },
    { "yes", IDS_SYNC_PASSPHRASE_CANCEL_YES },
    { "no", IDS_SYNC_PASSPHRASE_CANCEL_NO },
    { "sectionExplicitMessagePrefix", IDS_SYNC_PASSPHRASE_MSG_EXPLICIT_PREFIX },
    { "sectionExplicitMessagePostfix",
        IDS_SYNC_PASSPHRASE_MSG_EXPLICIT_POSTFIX },
    { "encryptedDataTypesTitle", IDS_SYNC_ENCRYPTION_DATA_TYPES_TITLE },
    { "encryptSensitiveOption", IDS_SYNC_ENCRYPT_SENSITIVE_DATA },
    { "encryptAllOption", IDS_SYNC_ENCRYPT_ALL_DATA },
    // TODO(rogerta): browser/resource/sync_promo/sync_promo.html and related
    // file may not be needed any more.  If not, then the following promo
    // strings can also be removed.
    { "promoPageTitle", IDS_SYNC_PROMO_TAB_TITLE },
    { "promoSkipButton", IDS_SYNC_PROMO_SKIP_BUTTON },
    { "promoAdvanced", IDS_SYNC_PROMO_ADVANCED },
    { "promoLearnMore", IDS_LEARN_MORE },
    { "promoTitleShort", IDS_SYNC_PROMO_MESSAGE_TITLE_SHORT },
    { "encryptionSectionTitle", IDS_SYNC_ENCRYPTION_SECTION_TITLE },
    { "basicEncryptionOption", IDS_SYNC_BASIC_ENCRYPTION_DATA },
    { "fullEncryptionOption", IDS_SYNC_FULL_ENCRYPTION_DATA },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "syncSetupOverlay", IDS_SYNC_SETUP_TITLE);
}

void SyncSetupHandler::DisplayConfigureSync(bool show_advanced,
                                            bool passphrase_failed) {
  // Should never call this when we are not signed in.
  DCHECK(!SigninManagerFactory::GetForProfile(
      GetProfile())->GetAuthenticatedUsername().empty());
  ProfileSyncService* service = GetSyncService();
  DCHECK(service);
  if (!service->sync_initialized()) {

#if !defined(OS_CHROMEOS)
    // When user tries to setup sync while the sync backend is not initialized,
    // kick the sync backend and wait for it to be ready and show spinner until
    // the backend gets ready.
    retry_on_signin_failure_ = false;
#endif

    service->UnsuppressAndStart();

    // See if it's even possible to bring up the sync backend - if not
    // (unrecoverable error?), don't bother displaying a spinner that will be
    // immediately closed because this leads to some ugly infinite UI loop (see
    // http://crbug.com/244769).
    if (SyncStartupTracker::GetSyncServiceState(GetProfile()) !=
        SyncStartupTracker::SYNC_STARTUP_ERROR) {
      DisplaySpinner();
    }

    // Start SyncSetupTracker to wait for sync to initialize.
    sync_startup_tracker_.reset(
        new SyncStartupTracker(GetProfile(), this));
    return;
  }

  // Should only get here if user is signed in and sync is initialized, so no
  // longer need a SigninTracker or SyncStartupTracker.
  signin_tracker_.reset();
  sync_startup_tracker_.reset();
  configuring_sync_ = true;
  DCHECK(service->sync_initialized()) <<
      "Cannot configure sync until the sync backend is initialized";

  // Setup args for the sync configure screen:
  //   showSyncEverythingPage: false to skip directly to the configure screen
  //   syncAllDataTypes: true if the user wants to sync everything
  //   syncNothing: true if the user wants to sync nothing
  //   <data_type>Registered: true if the associated data type is supported
  //   <data_type>Synced: true if the user wants to sync that specific data type
  //   encryptionEnabled: true if sync supports encryption
  //   encryptAllData: true if user wants to encrypt all data (not just
  //       passwords)
  //   usePassphrase: true if the data is encrypted with a secondary passphrase
  //   show_passphrase: true if a passphrase is needed to decrypt the sync data
  DictionaryValue args;

  // Tell the UI layer which data types are registered/enabled by the user.
  const syncer::ModelTypeSet registered_types =
      service->GetRegisteredDataTypes();
  const syncer::ModelTypeSet preferred_types =
      service->GetPreferredDataTypes();
  ModelTypeNameMap type_names = GetSelectableTypeNameMap();
  for (ModelTypeNameMap::const_iterator it = type_names.begin();
       it != type_names.end(); ++it) {
    syncer::ModelType sync_type = it->first;
    const std::string key_name = it->second;
    args.SetBoolean(key_name + "Registered",
                    registered_types.Has(sync_type));
    args.SetBoolean(key_name + "Synced", preferred_types.Has(sync_type));
  }
  browser_sync::SyncPrefs sync_prefs(GetProfile()->GetPrefs());
  args.SetBoolean("passphraseFailed", passphrase_failed);
  args.SetBoolean("showSyncEverythingPage", !show_advanced);
  args.SetBoolean("syncAllDataTypes", sync_prefs.HasKeepEverythingSynced());
  args.SetBoolean("syncNothing", false);  // Always false during initial setup.
  args.SetBoolean("encryptAllData", service->EncryptEverythingEnabled());

  // We call IsPassphraseRequired() here, instead of calling
  // IsPassphraseRequiredForDecryption(), because we want to show the passphrase
  // UI even if no encrypted data types are enabled.
  args.SetBoolean("showPassphrase", service->IsPassphraseRequired());
  // Keystore encryption is behind a flag. Only show the new encryption settings
  // if keystore encryption is enabled.
  args.SetBoolean("keystoreEncryptionEnabled", IsKeystoreEncryptionEnabled());

  // Set the proper encryption settings messages if keystore encryption is
  // enabled.
  if (IsKeystoreEncryptionEnabled()) {
    // To distinguish between FROZEN_IMPLICIT_PASSPHRASE and CUSTOM_PASSPHRASE
    // we only set usePassphrase for CUSTOM_PASSPHRASE.
    args.SetBoolean("usePassphrase",
                    service->GetPassphraseType() == syncer::CUSTOM_PASSPHRASE);
    base::Time passphrase_time = service->GetExplicitPassphraseTime();
    syncer::PassphraseType passphrase_type = service->GetPassphraseType();
    if (!passphrase_time.is_null()) {
      string16 passphrase_time_str = base::TimeFormatShortDate(passphrase_time);
      args.SetString(
          "enterPassphraseBody",
          GetStringFUTF16(IDS_SYNC_ENTER_PASSPHRASE_BODY_WITH_DATE,
                          passphrase_time_str));
      args.SetString(
          "enterGooglePassphraseBody",
          GetStringFUTF16(IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY_WITH_DATE,
                          passphrase_time_str));
      switch (passphrase_type) {
        case syncer::FROZEN_IMPLICIT_PASSPHRASE:
          args.SetString(
              "fullEncryptionBody",
              GetStringFUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_GOOGLE_WITH_DATE,
                              passphrase_time_str));
          break;
        case syncer::CUSTOM_PASSPHRASE:
          args.SetString(
              "fullEncryptionBody",
              GetStringFUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM_WITH_DATE,
                              passphrase_time_str));
          break;
        default:
          args.SetString(
              "fullEncryptionBody",
              GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM));
          break;
      }
    } else if (passphrase_type == syncer::CUSTOM_PASSPHRASE) {
      args.SetString(
          "fullEncryptionBody",
          GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM));
    } else {
      args.SetString(
          "fullEncryptionBody",
          GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_DATA));
    }
  } else {
    args.SetBoolean("usePassphrase", service->IsUsingSecondaryPassphrase());
  }

  StringValue page("configure");
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);

  // Make sure the tab used for the Gaia sign in does not cover the settings
  // tab.
  FocusUI();
}

void SyncSetupHandler::ConfigureSyncDone() {
  StringValue page("done");
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page);

  // Suppress the sync promo once the user signs into sync. This way the user
  // doesn't see the sync promo even if they sign out of sync later on.
  SyncPromoUI::SetUserSkippedSyncPromo(GetProfile());

  ProfileSyncService* service = GetSyncService();
  DCHECK(service);
  if (!service->HasSyncSetupCompleted()) {
    // This is the first time configuring sync, so log it.
    base::FilePath profile_file_path = GetProfile()->GetPath();
    ProfileMetrics::LogProfileSyncSignIn(profile_file_path);

    // We're done configuring, so notify ProfileSyncService that it is OK to
    // start syncing.
    service->SetSetupInProgress(false);
    service->SetSyncSetupCompleted();
  }
}

bool SyncSetupHandler::IsActiveLogin() const {
  // LoginUIService can be NULL if page is brought up in incognito mode
  // (i.e. if the user is running in guest mode in cros and brings up settings).
  LoginUIService* service = GetLoginUIService();
  return service && (service->current_login_ui() == this);
}

void SyncSetupHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "SyncSetupDidClosePage",
      base::Bind(&SyncSetupHandler::OnDidClosePage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupConfigure",
      base::Bind(&SyncSetupHandler::HandleConfigure,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupShowSetupUI",
      base::Bind(&SyncSetupHandler::HandleShowSetupUI,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("CloseTimeout",
      base::Bind(&SyncSetupHandler::HandleCloseTimeout,
                 base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "SyncSetupDoSignOutOnAuthError",
      base::Bind(&SyncSetupHandler::HandleDoSignOutOnAuthError,
                 base::Unretained(this)));
#else
  web_ui()->RegisterMessageCallback("SyncSetupStopSyncing",
      base::Bind(&SyncSetupHandler::HandleStopSyncing,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncSetupStartSignIn",
      base::Bind(&SyncSetupHandler::HandleStartSignin,
                 base::Unretained(this)));
#endif
}

#if !defined(OS_CHROMEOS)
void SyncSetupHandler::DisplayGaiaLogin() {
  DCHECK(!sync_startup_tracker_);
  // Advanced options are no longer being configured if the login screen is
  // visible. If the user exits the signin wizard after this without
  // configuring sync, CloseSyncSetup() will ensure they are logged out.
  configuring_sync_ = false;

  DisplayGaiaLoginInNewTabOrWindow();
  signin_tracker_.reset(new SigninTracker(GetProfile(), this));
}

void SyncSetupHandler::DisplayGaiaLoginInNewTabOrWindow() {
  DCHECK(!active_gaia_signin_tab_);
  GURL url(SyncPromoUI::GetSyncPromoURL(SyncPromoUI::SOURCE_SETTINGS, false));
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  if (!browser) {
    // Settings is not displayed in a browser window. Open a new window.
    browser = new Browser(Browser::CreateParams(
        Browser::TYPE_TABBED, GetProfile(), chrome::GetActiveDesktop()));
  }

  // If the signin manager already has an authenticated username, this is a
  // re-auth scenario, and we need to ensure that the user signs in with the
  // same email address.
  std::string email = SigninManagerFactory::GetForProfile(
      browser->profile())->GetAuthenticatedUsername();
  if (!email.empty()) {
    UMA_HISTOGRAM_ENUMERATION("Signin.Reauth",
                              signin::HISTOGRAM_SHOWN,
                              signin::HISTOGRAM_MAX);
    std::string fragment("Email=");
    fragment += email;
    GURL::Replacements replacements;
    replacements.SetRefStr(fragment);
    url = url.ReplaceComponents(replacements);
  }

  active_gaia_signin_tab_ = browser->OpenURL(
      content::OpenURLParams(url, content::Referrer(), SINGLETON_TAB,
                             content::PAGE_TRANSITION_AUTO_BOOKMARK, false));
  content::WebContentsObserver::Observe(active_gaia_signin_tab_);
}
#endif

bool SyncSetupHandler::PrepareSyncSetup() {

  // If the wizard is already visible, just focus that one.
  if (FocusExistingWizardIfPresent()) {
    if (!IsActiveLogin())
      CloseOverlay();
    return false;
  }

  // Notify services that login UI is now active.
  GetLoginUIService()->SetLoginUI(this);

  ProfileSyncService* service = GetSyncService();
  if (service)
    service->SetSetupInProgress(true);

  return true;
}

void SyncSetupHandler::DisplaySpinner() {
  configuring_sync_ = true;
  StringValue page("spinner");
  DictionaryValue args;

  const int kTimeoutSec = 30;
  DCHECK(!backend_start_timer_);
  backend_start_timer_.reset(new base::OneShotTimer<SyncSetupHandler>());
  backend_start_timer_->Start(FROM_HERE,
                              base::TimeDelta::FromSeconds(kTimeoutSec),
                              this, &SyncSetupHandler::DisplayTimeout);

  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);
}

// TODO(kochi): Handle error conditions other than timeout.
// http://crbug.com/128692
void SyncSetupHandler::DisplayTimeout() {
  DCHECK(!signin_tracker_);
  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();

  // Do not listen to sync startup events.
  sync_startup_tracker_.reset();

  StringValue page("timeout");
  DictionaryValue args;
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);
}

void SyncSetupHandler::RecordSignin() {
  // By default, do nothing - subclasses can override.
}

void SyncSetupHandler::OnDidClosePage(const ListValue* args) {
  CloseOverlay();
}

void SyncSetupHandler::SyncStartupFailed() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();

  // Just close the sync overlay (the idea is that the base settings page will
  // display the current err
  CloseOverlay();
}

void SyncSetupHandler::SyncStartupCompleted() {
  ProfileSyncService* service = GetSyncService();
  DCHECK(service->sync_initialized());

  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();

  DisplayConfigureSync(true, false);
}

void SyncSetupHandler::SigninFailed(const GoogleServiceAuthError& error) {
  DCHECK(!backend_start_timer_);

#if defined(OS_CHROMEOS)
  // TODO(peria): Show error dialog for prompting sign in and out on
  // Chrome OS. http://crbug.com/128692
  CloseOverlay();
#else
  last_signin_error_ = error;

  // Don't show the gaia sign in page again since there is no way to show the
  // user an error message.
  CloseSyncSetup();
#endif
}

Profile* SyncSetupHandler::GetProfile() const {
  return Profile::FromWebUI(web_ui());
}

ProfileSyncService* SyncSetupHandler::GetSyncService() const {
  Profile* profile = GetProfile();
  return profile->IsSyncAccessible() ?
      ProfileSyncServiceFactory::GetForProfile(GetProfile()) : NULL;
}

void SyncSetupHandler::SigninSuccess() {
  DCHECK(!backend_start_timer_);
  ProfileSyncService* service = GetSyncService();

#if !defined(OS_CHROMEOS)
  CloseGaiaSigninPage();
#endif

  // If we have signed in while sync is already setup, it must be due to some
  // kind of re-authentication flow. In that case, just close the signin dialog
  // rather than forcing the user to go through sync configuration.
  if (!service || service->HasSyncSetupCompleted()) {
    RecordSignin();
    CloseOverlay();
  } else {
    DisplayConfigureSync(false, false);
  }
}

void SyncSetupHandler::HandleConfigure(const ListValue* args) {
  DCHECK(!signin_tracker_);
  DCHECK(!sync_startup_tracker_);
  std::string json;
  if (!args->GetString(0, &json)) {
    NOTREACHED() << "Could not read JSON argument";
    return;
  }
  if (json.empty()) {
    NOTREACHED();
    return;
  }

  SyncConfigInfo configuration;
  if (!GetConfiguration(json, &configuration)) {
    // The page sent us something that we didn't understand.
    // This probably indicates a programming error.
    NOTREACHED();
    return;
  }

  // Start configuring the ProfileSyncService using the configuration passed
  // to us from the JS layer.
  ProfileSyncService* service = GetSyncService();

  // If the sync engine has shutdown for some reason, just close the sync
  // dialog.
  if (!service || !service->sync_initialized()) {
    CloseOverlay();
    return;
  }

  // Disable sync, but remain signed in if the user selected "Sync nothing" in
  // the advanced settings dialog. Note: In order to disable sync across
  // restarts on Chrome OS, we must call OnStopSyncingPermanently(), which
  // suppresses sync startup in addition to disabling it.
  if (configuration.sync_nothing) {
    ProfileSyncService::SyncEvent(
        ProfileSyncService::STOP_FROM_ADVANCED_DIALOG);
    CloseOverlay();
    service->OnStopSyncingPermanently();
    service->SetSetupInProgress(false);
    return;
  }

  // Note: Data encryption will not occur until configuration is complete
  // (when the PSS receives its CONFIGURE_DONE notification from the sync
  // backend), so the user still has a chance to cancel out of the operation
  // if (for example) some kind of passphrase error is encountered.
  if (configuration.encrypt_all)
    service->EnableEncryptEverything();

  bool passphrase_failed = false;
  if (!configuration.passphrase.empty()) {
    // We call IsPassphraseRequired() here (instead of
    // IsPassphraseRequiredForDecryption()) because the user may try to enter
    // a passphrase even though no encrypted data types are enabled.
    if (service->IsPassphraseRequired()) {
      // If we have pending keys, try to decrypt them with the provided
      // passphrase. We track if this succeeds or fails because a failed
      // decryption should result in an error even if there aren't any encrypted
      // data types.
      passphrase_failed =
          !service->SetDecryptionPassphrase(configuration.passphrase);
    } else {
      // OK, the user sent us a passphrase, but we don't have pending keys. So
      // it either means that the pending keys were resolved somehow since the
      // time the UI was displayed (re-encryption, pending passphrase change,
      // etc) or the user wants to re-encrypt.
      if (!configuration.passphrase_is_gaia &&
          !service->IsUsingSecondaryPassphrase()) {
        // User passed us a secondary passphrase, and the data is encrypted
        // with a GAIA passphrase so they must want to encrypt.
        service->SetEncryptionPassphrase(configuration.passphrase,
                                         ProfileSyncService::EXPLICIT);
      }
    }
  }

  bool user_was_prompted_for_passphrase =
      service->IsPassphraseRequiredForDecryption();
  service->OnUserChoseDatatypes(configuration.sync_everything,
                                configuration.data_types);

  // Need to call IsPassphraseRequiredForDecryption() *after* calling
  // OnUserChoseDatatypes() because the user may have just disabled the
  // encrypted datatypes (in which case we just want to exit, not prompt the
  // user for a passphrase).
  if (passphrase_failed || service->IsPassphraseRequiredForDecryption()) {
    // We need a passphrase, or the user's attempt to set a passphrase failed -
    // prompt them again. This covers a few subtle cases:
    // 1) The user enters an incorrect passphrase *and* disabled the encrypted
    //    data types. In that case we want to notify the user that the
    //    passphrase was incorrect even though there are no longer any encrypted
    //    types enabled (IsPassphraseRequiredForDecryption() == false).
    // 2) The user doesn't enter any passphrase. In this case, we won't call
    //    SetDecryptionPassphrase() (passphrase_failed == false), but we still
    //    want to display an error message to let the user know that their
    //    blank passphrase entry is not acceptable.
    // 3) The user just enabled an encrypted data type - in this case we don't
    //    want to display an "invalid passphrase" error, since it's the first
    //    time the user is seeing the prompt.
    DisplayConfigureSync(
        true, passphrase_failed || user_was_prompted_for_passphrase);
  } else {
    // No passphrase is required from the user so mark the configuration as
    // complete and close the sync setup overlay.
    ConfigureSyncDone();
  }

  ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_CUSTOMIZE);
  if (configuration.encrypt_all)
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_ENCRYPT);
  if (configuration.passphrase_is_gaia && !configuration.passphrase.empty())
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_PASSPHRASE);
  if (!configuration.sync_everything)
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_CHOOSE);
}

void SyncSetupHandler::HandleShowSetupUI(const ListValue* args) {
  ProfileSyncService* service = GetSyncService();
  DCHECK(service);

  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(GetProfile());
  if (signin->GetAuthenticatedUsername().empty()) {
    // For web-based signin, the signin page is not displayed in an overlay
    // on the settings page. So if we get here, it must be due to the user
    // cancelling signin (by reloading the sync settings page during initial
    // signin) or by directly navigating to settings/syncSetup
    // (http://crbug.com/229836). So just exit.
    DLOG(WARNING) << "Cannot display sync setup UI when not signed in";
    CloseOverlay();
    return;
  }

  // Bring up the existing wizard, or just display it on this page.
  if (!FocusExistingWizardIfPresent())
    OpenSyncSetup();
}

#if defined(OS_CHROMEOS)
// On ChromeOS, we need to sign out the user session to fix an auth error, so
// the user goes through the real signin flow to generate a new auth token.
void SyncSetupHandler::HandleDoSignOutOnAuthError(const ListValue* args) {
  DLOG(INFO) << "Signing out the user to fix a sync error.";
  chrome::AttemptUserExit();
}
#endif

#if !defined(OS_CHROMEOS)
void SyncSetupHandler::HandleStartSignin(const ListValue* args) {
  // Should only be called if the user is not already signed in.
  DCHECK(SigninManagerFactory::GetForProfile(GetProfile())->
      GetAuthenticatedUsername().empty());
  OpenSyncSetup();
}

void SyncSetupHandler::HandleStopSyncing(const ListValue* args) {
  if (GetSyncService())
    ProfileSyncService::SyncEvent(ProfileSyncService::STOP_FROM_OPTIONS);
#if !defined(OS_CHROMEOS)
  SigninManagerFactory::GetForProfile(GetProfile())->SignOut();
#endif
}
#endif

void SyncSetupHandler::HandleCloseTimeout(const ListValue* args) {
  CloseSyncSetup();
}

void SyncSetupHandler::CloseSyncSetup() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();

  // Clear the signin tracker before canceling sync setup, as it may incorrectly
  // flag a signin failure.
  bool was_signing_in = (signin_tracker_.get() != NULL);
  signin_tracker_.reset();
  sync_startup_tracker_.reset();

  // TODO(atwilson): Move UMA tracking of signin events out of sync module.
  ProfileSyncService* sync_service = GetSyncService();
  if (IsActiveLogin()) {
    // Don't log a cancel event if the sync setup dialog is being
    // automatically closed due to an auth error.
    if (!sync_service || (!sync_service->HasSyncSetupCompleted() &&
        sync_service->GetAuthError().state() == GoogleServiceAuthError::NONE)) {
      if (was_signing_in) {
        // TODO(rsimha): Remove this. Sync should not be logging sign in events.
        ProfileSyncService::SyncEvent(
            ProfileSyncService::CANCEL_DURING_SIGNON);
      } else if (configuring_sync_) {
        ProfileSyncService::SyncEvent(
            ProfileSyncService::CANCEL_DURING_CONFIGURE);
      } else {
        ProfileSyncService::SyncEvent(
            ProfileSyncService::CANCEL_FROM_SIGNON_WITHOUT_AUTH);
      }

      // If the user clicked "Cancel" while setting up sync, disable sync
      // because we don't want the sync backend to remain in the initialized
      // state. Note: In order to disable sync across restarts on Chrome OS, we
      // must call OnStopSyncingPermanently(), which suppresses sync startup in
      // addition to disabling it.
      if (sync_service) {
        DVLOG(1) << "Sync setup aborted by user action";
        sync_service->OnStopSyncingPermanently();
#if !defined(OS_CHROMEOS)
        // Sign out the user on desktop Chrome if they click cancel during
        // initial setup.
        // TODO(rsimha): Revisit this for M30. See http://crbug.com/252049.
        if (sync_service->FirstSetupInProgress())
          SigninManagerFactory::GetForProfile(GetProfile())->SignOut();
#endif
      }
    }

#if !defined(OS_CHROMEOS)
    // Let the various services know that we're no longer active.
    CloseGaiaSigninPage();
#endif

    GetLoginUIService()->LoginUIClosed(this);
  }

  // Alert the sync service anytime the sync setup dialog is closed. This can
  // happen due to the user clicking the OK or Cancel button, or due to the
  // dialog being closed by virtue of sync being disabled in the background.
  if (sync_service)
    sync_service->SetSetupInProgress(false);

#if !defined(OS_CHROMEOS)
  // Reset the attempted email address and error, otherwise the sync setup
  // overlay in the settings page will stay in whatever error state it was last
  // when it is reopened.
  last_attempted_user_email_.clear();
  last_signin_error_ = GoogleServiceAuthError::AuthErrorNone();
#endif

  configuring_sync_ = false;
}

void SyncSetupHandler::OpenSyncSetup() {
  if (!PrepareSyncSetup())
    return;

  // There are several different UI flows that can bring the user here:
  // 1) Signin promo.
  // 2) Normal signin through settings page (GetAuthenticatedUsername() is
  //    empty).
  // 3) Previously working credentials have expired.
  // 4) User is signed in, but has stopped sync via the google dashboard, and
  //    signout is prohibited by policy so we need to force a re-auth.
  // 5) User clicks [Advanced Settings] button on options page while already
  //    logged in.
  // 6) One-click signin (credentials are already available, so should display
  //    sync configure UI, not login UI).
  // 7) User re-enables sync after disabling it via advanced settings.
#if !defined(OS_CHROMEOS)
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(GetProfile());

  if (signin->GetAuthenticatedUsername().empty()) {
    // User is not logged in (cases 1-2). Display login UI.
    DisplayGaiaLogin();
    return;
  }

  if (SigninGlobalError::GetForProfile(GetProfile())->HasMenuItem()) {
    // Login has been specially requested because previously working credentials
    // have expired (case 3). Load the sync setup page with a spinner dialog,
    // and then display the gaia auth page. The user may abandon re-auth by
    // clicking cancel on the spinner dialog or closing the gaia login tab.
    StringValue page("spinner");
    web_ui()->CallJavascriptFunction("SyncSetupOverlay.showSyncSetupPage",
                                     page);
    DisplayGaiaLogin();
    return;
  }
#endif
  if (!GetSyncService()) {
    // This can happen if the user directly navigates to /settings/syncSetup.
    DLOG(WARNING) << "Cannot display sync UI when sync is disabled";
    CloseOverlay();
    return;
  }

  // User is already logged in. They must have brought up the config wizard
  // via the "Advanced..." button or through One-Click signin (cases 4-6), or
  // they are re-enabling sync after having disabled it (case 7).
  DisplayConfigureSync(true, false);
}

void SyncSetupHandler::OpenConfigureSync() {
  if (!PrepareSyncSetup())
    return;

  DisplayConfigureSync(true, false);
}

void SyncSetupHandler::FocusUI() {
  DCHECK(IsActiveLogin());
#if !defined(OS_CHROMEOS)
  // Bring the GAIA tab to the foreground if there is one.
  if (signin_tracker_ && active_gaia_signin_tab_) {
    BringTabToFront(active_gaia_signin_tab_);
    return;
  }
#endif
  WebContents* web_contents = web_ui()->GetWebContents();
  web_contents->GetDelegate()->ActivateContents(web_contents);
}

void SyncSetupHandler::CloseUI() {
  DCHECK(IsActiveLogin());
  CloseOverlay();
}

#if !defined(OS_CHROMEOS)
void SyncSetupHandler::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  DCHECK(active_gaia_signin_tab_);

  // If the user lands on a page outside of Gaia, assume they have navigated
  // away and are no longer thinking about signing in with this tab.  Treat
  // this as if the user closed the tab. However, don't actually close the tab
  // since the user is doing something with it.  Disconnect and forget about it
  // before closing down the sync setup.
  // The one exception is the expected continue URL.  If the user lands there,
  // this means sign in was successful.  Ignore the source parameter in the
  // continue URL since this user may have changed the state of the
  // "Let me choose what to sync" checkbox.
  const GURL& url = active_gaia_signin_tab_->GetURL();
  const GURL continue_url =
      SyncPromoUI::GetNextPageURLForSyncPromoURL(
          SyncPromoUI::GetSyncPromoURL(SyncPromoUI::SOURCE_SETTINGS,
                                       false));
  GURL::Replacements replacements;
  replacements.ClearQuery();

  if (!gaia::IsGaiaSignonRealm(url.GetOrigin()) &&
      url.ReplaceComponents(replacements) !=
          continue_url.ReplaceComponents(replacements)) {
    content::WebContentsObserver::Observe(NULL);
    active_gaia_signin_tab_ = NULL;
    CloseOverlay();
  }
}

void SyncSetupHandler::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK(active_gaia_signin_tab_);
  CloseOverlay();
}

// Private member functions.

void SyncSetupHandler::CloseGaiaSigninPage() {
  if (active_gaia_signin_tab_) {
    content::WebContentsObserver::Observe(NULL);

    // This can be invoked from a webui handler in the GAIA page (for example,
    // if the user clicks 'cancel' in the enterprise signin dialog), so
    // closing this tab in mid-handler can cause crashes. Instead, close it
    // via a task so we know we aren't in the middle of any webui code.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&CloseTab, base::Unretained(active_gaia_signin_tab_)));

    active_gaia_signin_tab_ = NULL;
  }
}
#endif

bool SyncSetupHandler::FocusExistingWizardIfPresent() {
  LoginUIService* service = GetLoginUIService();
  if (!service->current_login_ui())
    return false;
  service->current_login_ui()->FocusUI();
  return true;
}

LoginUIService* SyncSetupHandler::GetLoginUIService() const {
  return LoginUIServiceFactory::GetForProfile(GetProfile());
}

void SyncSetupHandler::CloseOverlay() {
  CloseSyncSetup();
  web_ui()->CallJavascriptFunction("OptionsPage.closeOverlay");
}
