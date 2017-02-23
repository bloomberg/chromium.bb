// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/sync_setup_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/sync_prefs.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "components/signin/core/browser/signin_manager_base.h"
#else
#include "components/signin/core/browser/signin_manager.h"
#endif

using browser_sync::ProfileSyncService;
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
  syncer::ModelTypeSet data_types;
  bool payments_integration_enabled;
  std::string passphrase;
  bool passphrase_is_gaia;
};

SyncConfigInfo::SyncConfigInfo()
    : encrypt_all(false),
      sync_everything(false),
      payments_integration_enabled(false),
      passphrase_is_gaia(false) {}

SyncConfigInfo::~SyncConfigInfo() {}

bool GetConfiguration(const std::string& json, SyncConfigInfo* config) {
  std::unique_ptr<base::Value> parsed_value = base::JSONReader::Read(json);
  base::DictionaryValue* result;
  if (!parsed_value || !parsed_value->GetAsDictionary(&result)) {
    DLOG(ERROR) << "GetConfiguration() not passed a Dictionary";
    return false;
  }

  if (!result->GetBoolean("syncAllDataTypes", &config->sync_everything)) {
    DLOG(ERROR) << "GetConfiguration() not passed a syncAllDataTypes value";
    return false;
  }

  if (!result->GetBoolean("paymentsIntegrationEnabled",
                          &config->payments_integration_enabled)) {
    DLOG(ERROR) << "GetConfiguration() not passed a paymentsIntegrationEnabled "
                << "value";
    return false;
  }

  syncer::ModelTypeNameMap type_names = syncer::GetUserSelectableTypeNameMap();

  for (syncer::ModelTypeNameMap::const_iterator it = type_names.begin();
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

}  // namespace

SyncSetupHandler::SyncSetupHandler()
    : configuring_sync_(false) {
}

SyncSetupHandler::~SyncSetupHandler() {
  // Just exit if running unit tests (no actual WebUI is attached).
  if (!web_ui())
    return;

  // This case is hit when the user performs a back navigation.
  CloseSyncSetup();
}

void SyncSetupHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  GetStaticLocalizedValues(localized_strings, web_ui());
}

void SyncSetupHandler::GetStaticLocalizedValues(
    base::DictionaryValue* localized_strings,
    content::WebUI* web_ui) {
  DCHECK(localized_strings);

  base::string16 product_name(GetStringUTF16(IDS_PRODUCT_NAME));
  localized_strings->SetString(
      "chooseDataTypesInstructions",
      GetStringFUTF16(IDS_SYNC_CHOOSE_DATATYPES_INSTRUCTIONS, product_name));
  localized_strings->SetString("autofillHelpURL", autofill::kHelpURL);
  localized_strings->SetString(
      "encryptionInstructions",
      GetStringFUTF16(IDS_SYNC_ENCRYPTION_INSTRUCTIONS, product_name));
  localized_strings->SetString(
      "encryptionHelpURL", chrome::kSyncEncryptionHelpURL);
  localized_strings->SetString(
      "encryptionSectionMessage",
      GetStringFUTF16(IDS_SYNC_ENCRYPTION_SECTION_MESSAGE, product_name));
  localized_strings->SetString(
      "personalizeGoogleServicesTitle",
      GetStringUTF16(IDS_SYNC_CONFIRMATION_PERSONALIZE_SERVICES_TITLE));
  localized_strings->SetString(
      "personalizeGoogleServicesMessage",
      GetStringFUTF16(
          IDS_SYNC_PERSONALIZE_GOOGLE_SERVICES_MESSAGE,
          base::ASCIIToUTF16(chrome::kGoogleAccountActivityControlsURL)));
  localized_strings->SetString(
      "passphraseRecover",
      GetStringFUTF16(
          IDS_SYNC_PASSPHRASE_RECOVER,
          base::ASCIIToUTF16(
              google_util::AppendGoogleLocaleParam(
                  GURL(chrome::kSyncGoogleDashboardURL),
                  g_browser_process->GetApplicationLocale()).spec())));
  localized_strings->SetString(
      "stopSyncingExplanation",
      l10n_util::GetStringFUTF16(
          IDS_SYNC_STOP_SYNCING_EXPLANATION_LABEL,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          base::ASCIIToUTF16(
              google_util::AppendGoogleLocaleParam(
                  GURL(chrome::kSyncGoogleDashboardURL),
                  g_browser_process->GetApplicationLocale()).spec())));
  localized_strings->SetString("deleteProfileLabel",
      l10n_util::GetStringUTF16(IDS_SYNC_STOP_DELETE_PROFILE_LABEL));
  localized_strings->SetString("stopSyncingTitle",
      l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_DIALOG_TITLE));
  localized_strings->SetString("stopSyncingConfirm",
        l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_CONFIRM_BUTTON_LABEL));

  localized_strings->SetString(
      "syncEverythingHelpURL", chrome::kSyncEverythingLearnMoreURL);
  localized_strings->SetString(
      "syncErrorHelpURL", chrome::kSyncErrorsHelpURL);

  static OptionsStringResource resources[] = {
      {"syncSetupConfigureTitle", IDS_SYNC_SETUP_CONFIGURE_TITLE},
      {"syncSetupSpinnerTitle", IDS_SYNC_SETUP_SPINNER_TITLE},
      {"syncSetupTimeoutTitle", IDS_SYNC_SETUP_TIME_OUT_TITLE},
      {"syncSetupTimeoutContent", IDS_SYNC_SETUP_TIME_OUT_CONTENT},
      {"errorLearnMore", IDS_LEARN_MORE},
      {"cancel", IDS_CANCEL},
      {"loginSuccess", IDS_SYNC_SUCCESS},
      {"settingUp", IDS_SYNC_LOGIN_SETTING_UP},
      {"syncAllDataTypes", IDS_SYNC_EVERYTHING},
      {"chooseDataTypes", IDS_SYNC_CHOOSE_DATATYPES},
      {"bookmarks", IDS_SYNC_DATATYPE_BOOKMARKS},
      {"preferences", IDS_SYNC_DATATYPE_PREFERENCES},
      {"autofill", IDS_SYNC_DATATYPE_AUTOFILL},
      {"themes", IDS_SYNC_DATATYPE_THEMES},
      {"passwords", IDS_SYNC_DATATYPE_PASSWORDS},
      {"extensions", IDS_SYNC_DATATYPE_EXTENSIONS},
      {"typedURLs", IDS_SYNC_DATATYPE_TYPED_URLS},
      {"apps", IDS_SYNC_DATATYPE_APPS},
      {"openTabs", IDS_SYNC_DATATYPE_TABS},
      {"enablePaymentsIntegration", IDS_AUTOFILL_USE_PAYMENTS_DATA},
      {"serviceUnavailableError", IDS_SYNC_SETUP_ABORTED_BY_PENDING_CLEAR},
      {"confirmLabel", IDS_SYNC_CONFIRM_PASSPHRASE_LABEL},
      {"emptyErrorMessage", IDS_SYNC_EMPTY_PASSPHRASE_ERROR},
      {"mismatchErrorMessage", IDS_SYNC_PASSPHRASE_MISMATCH_ERROR},
      {"customizeLinkLabel", IDS_SYNC_CUSTOMIZE_LINK_LABEL},
      {"confirmSyncPreferences", IDS_SYNC_CONFIRM_SYNC_PREFERENCES},
      {"syncEverything", IDS_SYNC_SYNC_EVERYTHING},
      {"useDefaultSettings", IDS_SYNC_USE_DEFAULT_SETTINGS},
      {"enterPassphraseBody", IDS_SYNC_ENTER_PASSPHRASE_BODY},
      {"enterGooglePassphraseBody", IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY},
      {"passphraseLabel", IDS_SYNC_PASSPHRASE_LABEL},
      {"incorrectPassphrase", IDS_SYNC_INCORRECT_PASSPHRASE},
      {"passphraseWarning", IDS_SYNC_PASSPHRASE_WARNING},
      {"yes", IDS_SYNC_PASSPHRASE_CANCEL_YES},
      {"no", IDS_SYNC_PASSPHRASE_CANCEL_NO},
      {"sectionExplicitMessagePrefix", IDS_SYNC_PASSPHRASE_MSG_EXPLICIT_PREFIX},
      {"sectionExplicitMessagePostfix",
       IDS_SYNC_PASSPHRASE_MSG_EXPLICIT_POSTFIX},
      // TODO(rogerta): browser/resource/sync_promo/sync_promo.html and related
      // file may not be needed any more.  If not, then the following promo
      // strings can also be removed.
      {"promoPageTitle", IDS_SYNC_PROMO_TAB_TITLE},
      {"promoSkipButton", IDS_SYNC_PROMO_SKIP_BUTTON},
      {"promoAdvanced", IDS_SYNC_PROMO_ADVANCED},
      {"promoLearnMore", IDS_LEARN_MORE},
      {"promoTitleShort", IDS_SYNC_PROMO_MESSAGE_TITLE_SHORT},
      {"encryptionSectionTitle", IDS_SYNC_ENCRYPTION_SECTION_TITLE},
      {"basicEncryptionOption", IDS_SYNC_BASIC_ENCRYPTION_DATA},
      {"fullEncryptionOption", IDS_SYNC_FULL_ENCRYPTION_DATA},
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "syncSetupOverlay", IDS_SYNC_SETUP_TITLE);
}

void SyncSetupHandler::ConfigureSyncDone() {
  base::StringValue page("done");
  web_ui()->CallJavascriptFunctionUnsafe("SyncSetupOverlay.showSyncSetupPage",
                                         page);

  // Suppress the sign in promo once the user starts sync. This way the user
  // doesn't see the sign in promo even if they sign out later on.
  signin::SetUserSkippedPromo(GetProfile());

  ProfileSyncService* service = GetSyncService();
  DCHECK(service);
  if (!service->IsFirstSetupComplete()) {
    // This is the first time configuring sync, so log it.
    base::FilePath profile_file_path = GetProfile()->GetPath();
    ProfileMetrics::LogProfileSyncSignIn(profile_file_path);

    // We're done configuring, so notify ProfileSyncService that it is OK to
    // start syncing.
    sync_blocker_.reset();
    service->SetFirstSetupComplete();
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
      "AttemptUserExit",
      base::Bind(&SyncSetupHandler::HandleAttemptUserExit,
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
void SyncSetupHandler::DisplayGaiaLogin(
    signin_metrics::AccessPoint access_point) {
  DCHECK(!sync_startup_tracker_);
  // Advanced options are no longer being configured if the login screen is
  // visible. If the user exits the signin wizard after this without
  // configuring sync, CloseSyncSetup() will ensure they are logged out.
  configuring_sync_ = false;
  DisplayGaiaLoginInNewTabOrWindow(access_point);
}

void SyncSetupHandler::DisplayGaiaLoginInNewTabOrWindow(
    signin_metrics::AccessPoint access_point) {
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  bool force_new_tab = false;
  if (!browser) {
    // Settings is not displayed in a browser window. Open a new window.
    browser = new Browser(
        Browser::CreateParams(Browser::TYPE_TABBED, GetProfile(), true));
    force_new_tab = true;
  }

  // If the signin manager already has an authenticated username, this is a
  // re-auth scenario, and we need to ensure that the user signs in with the
  // same email address.
  GURL url;
  if (SigninManagerFactory::GetForProfile(
      browser->profile())->IsAuthenticated()) {
    UMA_HISTOGRAM_ENUMERATION("Signin.Reauth",
                              signin_metrics::HISTOGRAM_REAUTH_SHOWN,
                              signin_metrics::HISTOGRAM_REAUTH_MAX);

    SigninErrorController* error_controller =
        SigninErrorControllerFactory::GetForProfile(browser->profile());
    DCHECK(error_controller->HasError());
    if (!force_new_tab) {
      browser->window()->ShowAvatarBubbleFromAvatarButton(
          BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH,
          signin::ManageAccountsParams(), access_point, false);
    } else {
      url = signin::GetReauthURL(
          access_point, signin_metrics::Reason::REASON_REAUTHENTICATION,
          browser->profile(), error_controller->error_account_id());
    }
  } else {
    if (!force_new_tab) {
      browser->window()->ShowAvatarBubbleFromAvatarButton(
          BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN,
          signin::ManageAccountsParams(), access_point, false);
    } else {
      url = signin::GetPromoURL(
          access_point, signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT,
          true);
    }
  }

  if (url.is_valid())
    chrome::ShowSingletonTab(browser, url);
}
#endif

bool SyncSetupHandler::PrepareSyncSetup() {
  // If the wizard is already visible, just focus that one.
  if (FocusExistingWizardIfPresent()) {
    if (!IsActiveLogin())
      CloseSyncSetup();
    return false;
  }

  // Notify services that login UI is now active.
  GetLoginUIService()->SetLoginUI(this);

  ProfileSyncService* service = GetSyncService();
  if (service)
    sync_blocker_ = service->GetSetupInProgressHandle();

  return true;
}

void SyncSetupHandler::DisplaySpinner() {
  configuring_sync_ = true;
  base::StringValue page("spinner");
  base::DictionaryValue args;

  const int kTimeoutSec = 30;
  DCHECK(!engine_start_timer_);
  engine_start_timer_.reset(new base::OneShotTimer());
  engine_start_timer_->Start(FROM_HERE,
                             base::TimeDelta::FromSeconds(kTimeoutSec), this,
                             &SyncSetupHandler::DisplayTimeout);

  web_ui()->CallJavascriptFunctionUnsafe("SyncSetupOverlay.showSyncSetupPage",
                                         page, args);
}

// TODO(kochi): Handle error conditions other than timeout.
// http://crbug.com/128692
void SyncSetupHandler::DisplayTimeout() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  engine_start_timer_.reset();

  // Do not listen to sync startup events.
  sync_startup_tracker_.reset();

  base::StringValue page("timeout");
  base::DictionaryValue args;
  web_ui()->CallJavascriptFunctionUnsafe("SyncSetupOverlay.showSyncSetupPage",
                                         page, args);
}

void SyncSetupHandler::OnDidClosePage(const base::ListValue* args) {
  CloseSyncSetup();
}

void SyncSetupHandler::SyncStartupFailed() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  engine_start_timer_.reset();

  // Just close the sync overlay (the idea is that the base settings page will
  // display the current error.)
  CloseUI();
}

void SyncSetupHandler::SyncStartupCompleted() {
  ProfileSyncService* service = GetSyncService();
  DCHECK(service->IsEngineInitialized());

  // Stop a timer to handle timeout in waiting for checking network connection.
  engine_start_timer_.reset();

  DisplayConfigureSync(false);
}

Profile* SyncSetupHandler::GetProfile() const {
  return Profile::FromWebUI(web_ui());
}

ProfileSyncService* SyncSetupHandler::GetSyncService() const {
  Profile* profile = GetProfile();
  return profile->IsSyncAllowed() ?
      ProfileSyncServiceFactory::GetForProfile(GetProfile()) : NULL;
}

void SyncSetupHandler::HandleConfigure(const base::ListValue* args) {
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
  if (!service || !service->IsEngineInitialized()) {
    CloseUI();
    return;
  }

  // Don't allow "encrypt all" if the ProfileSyncService doesn't allow it.
  // The UI is hidden, but the user may have enabled it e.g. by fiddling with
  // the web inspector.
  if (!service->IsEncryptEverythingAllowed())
    configuration.encrypt_all = false;

  // Note: Data encryption will not occur until configuration is complete
  // (when the PSS receives its CONFIGURE_DONE notification from the sync
  // engine), so the user still has a chance to cancel out of the operation
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

  PrefService* pref_service = GetProfile()->GetPrefs();
  pref_service->SetBoolean(autofill::prefs::kAutofillWalletImportEnabled,
                           configuration.payments_integration_enabled);

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
    DisplayConfigureSync(passphrase_failed || user_was_prompted_for_passphrase);
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

void SyncSetupHandler::HandleShowSetupUI(const base::ListValue* args) {
  if (!GetSyncService()) {
    DLOG(WARNING) << "Cannot display sync UI when sync is disabled";
    CloseUI();
    return;
  }

  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(GetProfile());
  if (!signin->IsAuthenticated()) {
    // For web-based signin, the signin page is not displayed in an overlay
    // on the settings page. So if we get here, it must be due to the user
    // cancelling signin (by reloading the sync settings page during initial
    // signin) or by directly navigating to settings/syncSetup
    // (http://crbug.com/229836). So just exit and go back to the settings page.
    DLOG(WARNING) << "Cannot display sync setup UI when not signed in";
    CloseUI();
    return;
  }

  // If a setup wizard is already present, but not on this page, close the
  // blank setup overlay on this page by showing the "done" page. This can
  // happen if the user navigates to chrome://settings/syncSetup in more than
  // one tab. See crbug.com/261566.
  // Note: The following block will transfer focus to the existing wizard.
  if (IsExistingWizardPresent() && !IsActiveLogin())
    CloseUI();

  // If a setup wizard is present on this page or another, bring it to focus.
  // Otherwise, display a new one on this page.
  if (!FocusExistingWizardIfPresent())
    OpenSyncSetup(false /* creating_supervised_user */);
}

#if defined(OS_CHROMEOS)
// On ChromeOS, we need to sign out the user session to fix an auth error, so
// the user goes through the real signin flow to generate a new auth token.
void SyncSetupHandler::HandleAttemptUserExit(const base::ListValue* args) {
  chrome::AttemptUserExit();
}
#endif

#if !defined(OS_CHROMEOS)
void SyncSetupHandler::HandleStartSignin(const base::ListValue* args) {
  // Should only be called if the user is not already signed in or has an auth
  // error.
  DCHECK(
      !SigninManagerFactory::GetForProfile(GetProfile())->IsAuthenticated() ||
      SigninErrorControllerFactory::GetForProfile(GetProfile())->HasError());
  bool creating_supervised_user = false;
  args->GetBoolean(0, &creating_supervised_user);
  OpenSyncSetup(creating_supervised_user);
}

void SyncSetupHandler::HandleStopSyncing(const base::ListValue* args) {
  if (GetSyncService())
    ProfileSyncService::SyncEvent(ProfileSyncService::STOP_FROM_OPTIONS);

  bool delete_profile = false;
  args->GetBoolean(0, &delete_profile);
  signin_metrics::SignoutDelete delete_metric =
      delete_profile ? signin_metrics::SignoutDelete::DELETED
                     : signin_metrics::SignoutDelete::KEEPING;
  SigninManagerFactory::GetForProfile(GetProfile())
      ->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS, delete_metric);

  if (delete_profile) {
    // Do as BrowserOptionsHandler::DeleteProfile().
    webui::DeleteProfileAtPath(GetProfile()->GetPath(),
                               web_ui(),
                               ProfileMetrics::DELETE_PROFILE_SETTINGS);
  }
}
#endif

void SyncSetupHandler::HandleCloseTimeout(const base::ListValue* args) {
  CloseSyncSetup();
}

void SyncSetupHandler::CloseSyncSetup() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  engine_start_timer_.reset();

  // Clear the sync startup tracker, since the setup wizard is being closed.
  sync_startup_tracker_.reset();

  ProfileSyncService* sync_service = GetSyncService();
  if (IsActiveLogin()) {
    // Don't log a cancel event if the sync setup dialog is being
    // automatically closed due to an auth error.
    if (!sync_service || (!sync_service->IsFirstSetupComplete() &&
                          sync_service->GetAuthError().state() ==
                              GoogleServiceAuthError::NONE)) {
      if (configuring_sync_) {
        ProfileSyncService::SyncEvent(
            ProfileSyncService::CANCEL_DURING_CONFIGURE);

        // If the user clicked "Cancel" while setting up sync, disable sync
        // because we don't want the sync engine to remain in the
        // first-setup-incomplete state.
        // Note: In order to disable sync across restarts on Chrome OS,
        // we must call RequestStop(CLEAR_DATA), which suppresses sync startup
        // in addition to disabling it.
        if (sync_service) {
          DVLOG(1) << "Sync setup aborted by user action";
          sync_service->RequestStop(ProfileSyncService::CLEAR_DATA);
  #if !defined(OS_CHROMEOS)
          // Sign out the user on desktop Chrome if they click cancel during
          // initial setup.
          // TODO(rsimha): Revisit this for M30. See http://crbug.com/252049.
          if (sync_service->IsFirstSetupInProgress()) {
            SigninManagerFactory::GetForProfile(GetProfile())
                ->SignOut(signin_metrics::ABORT_SIGNIN,
                          signin_metrics::SignoutDelete::IGNORE_METRIC);
          }
  #endif
        }
      }
    }
  }

  LoginUIService* service = GetLoginUIService();
  if (service)
    service->LoginUIClosed(this);

  // Alert the sync service anytime the sync setup dialog is closed. This can
  // happen due to the user clicking the OK or Cancel button, or due to the
  // dialog being closed by virtue of sync being disabled in the background.
  sync_blocker_.reset();

  configuring_sync_ = false;
}

void SyncSetupHandler::OpenSyncSetup(bool creating_supervised_user) {
  if (!PrepareSyncSetup())
    return;

  // There are several different UI flows that can bring the user here:
  // 1) Signin promo.
  // 2) Normal signin through settings page (IsAuthenticated() is false).
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

  if (!signin->IsAuthenticated() ||
      SigninErrorControllerFactory::GetForProfile(GetProfile())->HasError()) {
    // User is not logged in (cases 1-2), or login has been specially requested
    // because previously working credentials have expired (case 3). Close sync
    // setup including any visible overlays, and display the gaia auth page.
    // Control will be returned to the sync settings page once auth is complete.
    CloseUI();
    DisplayGaiaLogin(
        creating_supervised_user ?
            signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER :
            signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
    return;
  }
#endif
  if (!GetSyncService()) {
    // This can happen if the user directly navigates to /settings/syncSetup.
    DLOG(WARNING) << "Cannot display sync UI when sync is disabled";
    CloseUI();
    return;
  }

  // User is already logged in. They must have brought up the config wizard
  // via the "Advanced..." button or through One-Click signin (cases 4-6), or
  // they are re-enabling sync after having disabled it (case 7).
  DisplayConfigureSync(false);
}

void SyncSetupHandler::OpenConfigureSync() {
  if (!PrepareSyncSetup())
    return;

  DisplayConfigureSync(false);
}

void SyncSetupHandler::FocusUI() {
  DCHECK(IsActiveLogin());
  WebContents* web_contents = web_ui()->GetWebContents();
  web_contents->GetDelegate()->ActivateContents(web_contents);
}

void SyncSetupHandler::CloseUI() {
  CloseSyncSetup();
  base::StringValue page("done");
  web_ui()->CallJavascriptFunctionUnsafe("SyncSetupOverlay.showSyncSetupPage",
                                         page);
}

bool SyncSetupHandler::IsExistingWizardPresent() {
  LoginUIService* service = GetLoginUIService();
  DCHECK(service);
  return service->current_login_ui() != NULL;
}

bool SyncSetupHandler::FocusExistingWizardIfPresent() {
  if (!IsExistingWizardPresent())
    return false;

  LoginUIService* service = GetLoginUIService();
  DCHECK(service);
  service->current_login_ui()->FocusUI();
  return true;
}

void SyncSetupHandler::DisplayConfigureSync(bool passphrase_failed) {
  // Should never call this when we are not signed in.
  DCHECK(SigninManagerFactory::GetForProfile(
      GetProfile())->IsAuthenticated());
  ProfileSyncService* service = GetSyncService();
  DCHECK(service);
  if (!service->IsEngineInitialized()) {
    service->RequestStart();

    // See if it's even possible to bring up the sync engine - if not
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
  // longer need a SyncStartupTracker.
  sync_startup_tracker_.reset();
  configuring_sync_ = true;
  DCHECK(service->IsEngineInitialized())
      << "Cannot configure sync until the sync engine is initialized";

  // Setup args for the sync configure screen:
  //   syncAllDataTypes: true if the user wants to sync everything
  //   syncNothing: true if the user wants to sync nothing
  //   <data_type>Registered: true if the associated data type is supported
  //   <data_type>Synced: true if the user wants to sync that specific data type
  //   paymentsIntegrationEnabled: true if the user wants Payments integration
  //   encryptionEnabled: true if sync supports encryption
  //   encryptAllData: true if user wants to encrypt all data (not just
  //       passwords)
  //   usePassphrase: true if the data is encrypted with a secondary passphrase
  //   show_passphrase: true if a passphrase is needed to decrypt the sync data
  base::DictionaryValue args;

  // Tell the UI layer which data types are registered/enabled by the user.
  const syncer::ModelTypeSet registered_types =
      service->GetRegisteredDataTypes();
  const syncer::ModelTypeSet preferred_types = service->GetPreferredDataTypes();
  const syncer::ModelTypeSet enforced_types = service->GetForcedDataTypes();
  syncer::ModelTypeNameMap type_names = syncer::GetUserSelectableTypeNameMap();
  for (syncer::ModelTypeNameMap::const_iterator it = type_names.begin();
       it != type_names.end(); ++it) {
    syncer::ModelType sync_type = it->first;
    const std::string key_name = it->second;
    args.SetBoolean(key_name + "Registered", registered_types.Has(sync_type));
    args.SetBoolean(key_name + "Synced", preferred_types.Has(sync_type));
    args.SetBoolean(key_name + "Enforced", enforced_types.Has(sync_type));
    // TODO(treib): How do we want to handle pref groups, i.e. when only some of
    // the sync types behind a checkbox are force-enabled? crbug.com/403326
  }
  PrefService* pref_service = GetProfile()->GetPrefs();
  syncer::SyncPrefs sync_prefs(pref_service);
  args.SetBoolean("passphraseFailed", passphrase_failed);
  args.SetBoolean("syncAllDataTypes", sync_prefs.HasKeepEverythingSynced());
  args.SetBoolean("syncNothing", false);  // Always false during initial setup.
  args.SetBoolean(
      "paymentsIntegrationEnabled",
      pref_service->GetBoolean(autofill::prefs::kAutofillWalletImportEnabled));
  args.SetBoolean("encryptAllData", service->IsEncryptEverythingEnabled());
  args.SetBoolean("encryptAllDataAllowed",
                  service->IsEncryptEverythingAllowed());

  // We call IsPassphraseRequired() here, instead of calling
  // IsPassphraseRequiredForDecryption(), because we want to show the passphrase
  // UI even if no encrypted data types are enabled.
  args.SetBoolean("showPassphrase", service->IsPassphraseRequired());

  // To distinguish between FROZEN_IMPLICIT_PASSPHRASE and CUSTOM_PASSPHRASE
  // we only set usePassphrase for PassphraseType::CUSTOM_PASSPHRASE.
  args.SetBoolean("usePassphrase",
                  service->GetPassphraseType() ==
                      syncer::PassphraseType::CUSTOM_PASSPHRASE);
  base::Time passphrase_time = service->GetExplicitPassphraseTime();
  syncer::PassphraseType passphrase_type = service->GetPassphraseType();
  if (!passphrase_time.is_null()) {
    base::string16 passphrase_time_str =
        base::TimeFormatShortDate(passphrase_time);
    args.SetString(
        "enterPassphraseBody",
        GetStringFUTF16(IDS_SYNC_ENTER_PASSPHRASE_BODY_WITH_DATE,
                        passphrase_time_str));
    args.SetString(
        "enterGooglePassphraseBody",
        GetStringFUTF16(IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY_WITH_DATE,
                        passphrase_time_str));
    switch (passphrase_type) {
      case syncer::PassphraseType::FROZEN_IMPLICIT_PASSPHRASE:
        args.SetString(
            "fullEncryptionBody",
            GetStringFUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_GOOGLE_WITH_DATE,
                            passphrase_time_str));
        break;
      case syncer::PassphraseType::CUSTOM_PASSPHRASE:
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
  } else if (passphrase_type == syncer::PassphraseType::CUSTOM_PASSPHRASE) {
    args.SetString(
        "fullEncryptionBody",
        GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM));
  } else {
    args.SetString(
        "fullEncryptionBody",
        GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_DATA));
  }

  base::StringValue page("configure");
  web_ui()->CallJavascriptFunctionUnsafe("SyncSetupOverlay.showSyncSetupPage",
                                         page, args);

  // Make sure the tab used for the Gaia sign in does not cover the settings
  // tab.
  FocusUI();
}

LoginUIService* SyncSetupHandler::GetLoginUIService() const {
  return LoginUIServiceFactory::GetForProfile(GetProfile());
}
