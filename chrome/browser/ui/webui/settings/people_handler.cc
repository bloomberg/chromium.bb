// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/people_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/options/options_handlers_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "grit/components_strings.h"
#include "net/base/url_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/user_manager/user_manager.h"
#else
#include "components/signin/core/browser/signin_manager.h"
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
      passphrase_is_gaia(false) {}

SyncConfigInfo::~SyncConfigInfo() {}

bool GetConfiguration(const std::string& json, SyncConfigInfo* config) {
  scoped_ptr<base::Value> parsed_value = base::JSONReader::Read(json);
  base::DictionaryValue* result;
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

// Retrieves the
void GetAccountNameAndIcon(const Profile& profile,
                           std::string* name,
                           std::string* icon_url) {
  DCHECK(name);
  DCHECK(icon_url);

#if defined(OS_CHROMEOS)
  *name = profile.GetProfileUserName();
  if (name->empty()) {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(&profile);
    if (user && (user->GetType() != user_manager::USER_TYPE_GUEST))
      *name = user->email();
  }
  if (!name->empty())
    *name = gaia::SanitizeEmail(gaia::CanonicalizeEmail(*name));

  // Get image as data URL instead of using chrome://userimage source to avoid
  // issues with caching.
  const AccountId account_id(AccountId::FromUserEmail(*name));
  scoped_refptr<base::RefCountedMemory> image =
      chromeos::options::UserImageSource::GetUserImage(account_id);
  *icon_url = webui::GetPngDataUrl(image->front(), image->size());
#else   // !defined(OS_CHROMEOS)
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  ProfileAttributesEntry* entry = nullptr;
  if (cache.GetProfileAttributesWithPath(profile.GetPath(), &entry)) {
    *name = base::UTF16ToUTF8(entry->GetName());

    if (entry->IsUsingGAIAPicture() && entry->GetGAIAPicture()) {
      gfx::Image icon =
          profiles::GetAvatarIconForWebUI(entry->GetAvatarIcon(), true);
      *icon_url = webui::GetBitmapDataUrl(icon.AsBitmap());
    } else {
      *icon_url =
          profiles::GetDefaultAvatarIconUrl(entry->GetAvatarIconIndex());
    }
  }
#endif  // defined(OS_CHROMEOS)
}

}  // namespace

namespace settings {

PeopleHandler::PeopleHandler(Profile* profile)
    : profile_(profile),
      configuring_sync_(false),
      sync_service_observer_(this) {
  PrefService* prefs = profile_->GetPrefs();
  profile_pref_registrar_.Init(prefs);
  profile_pref_registrar_.Add(
      prefs::kSigninAllowed,
      base::Bind(&PeopleHandler::OnSigninAllowedPrefChange,
                 base::Unretained(this)));

  ProfileSyncService* sync_service(
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_));
  if (sync_service)
    sync_service_observer_.Add(sync_service);

  g_browser_process->profile_manager()->GetProfileInfoCache().AddObserver(this);
}

PeopleHandler::~PeopleHandler() {
  g_browser_process->profile_manager()->
      GetProfileInfoCache().RemoveObserver(this);

  // Early exit if running unit tests (no actual WebUI is attached).
  if (!web_ui())
    return;

  // This case is hit when the user performs a back navigation.
  CloseSyncSetup();
}

void PeopleHandler::ConfigureSyncDone() {
  base::StringValue page("done");
  web_ui()->CallJavascriptFunction("settings.SyncPrivateApi.showSyncSetupPage",
                                   page);

  // Suppress the sign in promo once the user starts sync. This way the user
  // doesn't see the sign in promo even if they sign out later on.
  signin::SetUserSkippedPromo(profile_);

  ProfileSyncService* service = GetSyncService();
  DCHECK(service);
  if (!service->IsFirstSetupComplete()) {
    // This is the first time configuring sync, so log it.
    base::FilePath profile_file_path = profile_->GetPath();
    ProfileMetrics::LogProfileSyncSignIn(profile_file_path);

    // We're done configuring, so notify ProfileSyncService that it is OK to
    // start syncing.
    service->SetSetupInProgress(false);
    service->SetFirstSetupComplete();
  }
}

bool PeopleHandler::IsActiveLogin() const {
  // LoginUIService can be nullptr if page is brought up in incognito mode
  // (i.e. if the user is running in guest mode in cros and brings up settings).
  LoginUIService* service = GetLoginUIService();
  return service && (service->current_login_ui() == this);
}

void PeopleHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getProfileInfo",
      base::Bind(&PeopleHandler::HandleGetProfileInfo, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupDidClosePage",
      base::Bind(&PeopleHandler::OnDidClosePage, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupConfigure",
      base::Bind(&PeopleHandler::HandleConfigure, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupShowSetupUI",
      base::Bind(&PeopleHandler::HandleShowSetupUI, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupCloseTimeout",
      base::Bind(&PeopleHandler::HandleCloseTimeout, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupGetSyncStatus",
      base::Bind(&PeopleHandler::HandleGetSyncStatus, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupManageOtherPeople",
      base::Bind(&PeopleHandler::HandleManageOtherPeople,
                 base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "SyncSetupDoSignOutOnAuthError",
      base::Bind(&PeopleHandler::HandleDoSignOutOnAuthError,
                 base::Unretained(this)));
#else
  web_ui()->RegisterMessageCallback(
      "SyncSetupStopSyncing",
      base::Bind(&PeopleHandler::HandleStopSyncing, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupStartSignIn",
      base::Bind(&PeopleHandler::HandleStartSignin, base::Unretained(this)));
#endif
}

#if !defined(OS_CHROMEOS)
void PeopleHandler::DisplayGaiaLogin(signin_metrics::AccessPoint access_point) {
  DCHECK(!sync_startup_tracker_);
  // Advanced options are no longer being configured if the login screen is
  // visible. If the user exits the signin wizard after this without
  // configuring sync, CloseSyncSetup() will ensure they are logged out.
  configuring_sync_ = false;
  DisplayGaiaLoginInNewTabOrWindow(access_point);
}

void PeopleHandler::DisplayGaiaLoginInNewTabOrWindow(
    signin_metrics::AccessPoint access_point) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  bool force_new_tab = false;
  if (!browser) {
    // Settings is not displayed in a browser window. Open a new window.
    browser =
        new Browser(Browser::CreateParams(Browser::TYPE_TABBED, profile_));
    force_new_tab = true;
  }

  // If the signin manager already has an authenticated username, this is a
  // re-auth scenario, and we need to ensure that the user signs in with the
  // same email address.
  GURL url;
  if (SigninManagerFactory::GetForProfile(browser->profile())
          ->IsAuthenticated()) {
    UMA_HISTOGRAM_ENUMERATION("Signin.Reauth",
                              signin_metrics::HISTOGRAM_REAUTH_SHOWN,
                              signin_metrics::HISTOGRAM_REAUTH_MAX);

    SigninErrorController* error_controller =
        SigninErrorControllerFactory::GetForProfile(browser->profile());
    DCHECK(error_controller->HasError());
    if (!force_new_tab) {
      browser->window()->ShowAvatarBubbleFromAvatarButton(
          BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH,
          signin::ManageAccountsParams(), access_point);
    } else {
      url = signin::GetReauthURL(
          access_point, signin_metrics::Reason::REASON_REAUTHENTICATION,
          browser->profile(), error_controller->error_account_id());
    }
  } else {
    if (!force_new_tab) {
      browser->window()->ShowAvatarBubbleFromAvatarButton(
          BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN,
          signin::ManageAccountsParams(), access_point);
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

bool PeopleHandler::PrepareSyncSetup() {
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
    service->SetSetupInProgress(true);

  return true;
}

void PeopleHandler::DisplaySpinner() {
  configuring_sync_ = true;
  base::StringValue page("spinner");
  base::DictionaryValue args;

  const int kTimeoutSec = 30;
  DCHECK(!backend_start_timer_);
  backend_start_timer_.reset(new base::OneShotTimer());
  backend_start_timer_->Start(FROM_HERE,
                              base::TimeDelta::FromSeconds(kTimeoutSec), this,
                              &PeopleHandler::DisplayTimeout);

  web_ui()->CallJavascriptFunction("settings.SyncPrivateApi.showSyncSetupPage",
                                   page, args);
}

// TODO(kochi): Handle error conditions other than timeout.
// http://crbug.com/128692
void PeopleHandler::DisplayTimeout() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();

  // Do not listen to sync startup events.
  sync_startup_tracker_.reset();

  base::StringValue page("timeout");
  base::DictionaryValue args;
  web_ui()->CallJavascriptFunction("settings.SyncPrivateApi.showSyncSetupPage",
                                   page, args);
}

void PeopleHandler::OnDidClosePage(const base::ListValue* args) {
  CloseSyncSetup();
}

void PeopleHandler::SyncStartupFailed() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();

  // Just close the sync overlay (the idea is that the base settings page will
  // display the current error.)
  CloseUI();
}

void PeopleHandler::SyncStartupCompleted() {
  ProfileSyncService* service = GetSyncService();
  DCHECK(service->IsBackendInitialized());

  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();

  DisplayConfigureSync(false);
}

ProfileSyncService* PeopleHandler::GetSyncService() const {
  return profile_->IsSyncAllowed()
             ? ProfileSyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}

void PeopleHandler::HandleGetProfileInfo(const base::ListValue* args) {
  std::string name;
  std::string icon_url;
  GetAccountNameAndIcon(*profile_, &name, &icon_url);

  web_ui()->CallJavascriptFunction("settings.SyncPrivateApi.receiveProfileInfo",
                                   base::StringValue(name),
                                   base::StringValue(icon_url));
}

void PeopleHandler::HandleConfigure(const base::ListValue* args) {
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
  if (!service || !service->IsBackendInitialized()) {
    CloseUI();
    return;
  }

  // Disable sync, but remain signed in if the user selected "Sync nothing" in
  // the advanced settings dialog. Note: In order to disable sync across
  // restarts on Chrome OS, we must call RequestStop(CLEAR_DATA), which
  // suppresses sync startup in addition to disabling it.
  if (configuration.sync_nothing) {
    ProfileSyncService::SyncEvent(
        ProfileSyncService::STOP_FROM_ADVANCED_DIALOG);
    CloseUI();
    service->RequestStop(ProfileSyncService::CLEAR_DATA);
    service->SetSetupInProgress(false);
    return;
  }

  // Don't allow "encrypt all" if the ProfileSyncService doesn't allow it.
  // The UI is hidden, but the user may have enabled it e.g. by fiddling with
  // the web inspector.
  if (!service->IsEncryptEverythingAllowed())
    configuration.encrypt_all = false;

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

void PeopleHandler::HandleShowSetupUI(const base::ListValue* args) {
  if (!GetSyncService()) {
    CloseUI();
    return;
  }

  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile_);
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
    OpenSyncSetup(args);
}

#if defined(OS_CHROMEOS)
// On ChromeOS, we need to sign out the user session to fix an auth error, so
// the user goes through the real signin flow to generate a new auth token.
void PeopleHandler::HandleDoSignOutOnAuthError(const base::ListValue* args) {
  DVLOG(1) << "Signing out the user to fix a sync error.";
  chrome::AttemptUserExit();
}
#endif

#if !defined(OS_CHROMEOS)
void PeopleHandler::HandleStartSignin(const base::ListValue* args) {
  // Should only be called if the user is not already signed in.
  DCHECK(!SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated());
  OpenSyncSetup(args);
}

void PeopleHandler::HandleStopSyncing(const base::ListValue* args) {
  if (GetSyncService())
    ProfileSyncService::SyncEvent(ProfileSyncService::STOP_FROM_OPTIONS);

  bool delete_profile = false;
  args->GetBoolean(0, &delete_profile);
  signin_metrics::SignoutDelete delete_metric =
      delete_profile ? signin_metrics::SignoutDelete::DELETED
                     : signin_metrics::SignoutDelete::KEEPING;
  SigninManagerFactory::GetForProfile(profile_)
      ->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS, delete_metric);

  if (delete_profile) {
    // Do as BrowserOptionsHandler::DeleteProfile().
    options::helper::DeleteProfileAtPath(profile_->GetPath(), web_ui());
  }
}
#endif

void PeopleHandler::HandleCloseTimeout(const base::ListValue* args) {
  CloseSyncSetup();
}

void PeopleHandler::HandleGetSyncStatus(const base::ListValue* /* args */) {
  UpdateSyncState();
}

void PeopleHandler::HandleManageOtherPeople(const base::ListValue* /* args */) {
  UserManager::Show(base::FilePath(), profiles::USER_MANAGER_NO_TUTORIAL,
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
}

void PeopleHandler::CloseSyncSetup() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();

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
        // because we don't want the sync backend to remain in the
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
            SigninManagerFactory::GetForProfile(profile_)
                ->SignOut(signin_metrics::ABORT_SIGNIN,
                          signin_metrics::SignoutDelete::IGNORE_METRIC);
          }
#endif
        }
      }
    }

    GetLoginUIService()->LoginUIClosed(this);
  }

  // Alert the sync service anytime the sync setup dialog is closed. This can
  // happen due to the user clicking the OK or Cancel button, or due to the
  // dialog being closed by virtue of sync being disabled in the background.
  if (sync_service)
    sync_service->SetSetupInProgress(false);

  configuring_sync_ = false;
}

void PeopleHandler::OpenSyncSetup(const base::ListValue* args) {
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
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile_);

  if (!signin->IsAuthenticated() ||
      SigninErrorControllerFactory::GetForProfile(profile_)->HasError()) {
    // User is not logged in (cases 1-2), or login has been specially requested
    // because previously working credentials have expired (case 3). Close sync
    // setup including any visible overlays, and display the gaia auth page.
    // Control will be returned to the sync settings page once auth is complete.
    CloseUI();
    if (args) {
      std::string access_point = base::UTF16ToUTF8(ExtractStringValue(args));
      if (access_point == "access-point-supervised-user") {
        DisplayGaiaLogin(
            signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER);
        return;
      }
    }
    DisplayGaiaLogin(signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
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

void PeopleHandler::OpenConfigureSync() {
  if (!PrepareSyncSetup())
    return;

  DisplayConfigureSync(false);
}

void PeopleHandler::FocusUI() {
  DCHECK(IsActiveLogin());
  WebContents* web_contents = web_ui()->GetWebContents();
  web_contents->GetDelegate()->ActivateContents(web_contents);
}

void PeopleHandler::CloseUI() {
  CloseSyncSetup();
  base::StringValue page("done");
  web_ui()->CallJavascriptFunction("settings.SyncPrivateApi.showSyncSetupPage",
                                   page);
}

void PeopleHandler::GoogleSigninSucceeded(const std::string& /* account_id */,
                                          const std::string& /* username */,
                                          const std::string& /* password */) {
  UpdateSyncState();
}

void PeopleHandler::GoogleSignedOut(const std::string& /* account_id */,
                                    const std::string& /* username */) {
  UpdateSyncState();
}

void PeopleHandler::OnStateChanged() {
  UpdateSyncState();
}

void PeopleHandler::OnProfileNameChanged(
    const base::FilePath& /* profile_path */,
    const base::string16& /* old_profile_name */) {
  HandleGetProfileInfo(nullptr);
}

void PeopleHandler::OnProfileAvatarChanged(
    const base::FilePath& /* profile_path */) {
  HandleGetProfileInfo(nullptr);
}

scoped_ptr<base::DictionaryValue> PeopleHandler::GetSyncStateDictionary() {
  // The items which are to be written into |sync_status| are also described in
  // chrome/browser/resources/options/browser_options.js in @typedef
  // for SyncStatus. Please update it whenever you add or remove any keys here.
  scoped_ptr<base::DictionaryValue> sync_status(new base::DictionaryValue);
  if (profile_->IsGuestSession()) {
    // Cannot display signin status when running in guest mode on chromeos
    // because there is no SigninManager.
    sync_status->SetBoolean("signinAllowed", false);
    return sync_status;
  }

  sync_status->SetBoolean("supervisedUser", profile_->IsSupervised());
  sync_status->SetBoolean("childUser", profile_->IsChild());

  bool signout_prohibited = false;
#if !defined(OS_CHROMEOS)
  // Signout is not allowed if the user has policy (crbug.com/172204).
  signout_prohibited =
      SigninManagerFactory::GetForProfile(profile_)->IsSignoutProhibited();
#endif

  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile_);
  DCHECK(signin);
  sync_status->SetBoolean("signoutAllowed", !signout_prohibited);
  sync_status->SetBoolean("signinAllowed", signin->IsSigninAllowed());
  sync_status->SetBoolean("syncSystemEnabled", (service != nullptr));
  sync_status->SetBoolean("setupCompleted",
                          service && service->IsFirstSetupComplete());
  sync_status->SetBoolean(
      "setupInProgress",
      service && !service->IsManaged() && service->IsFirstSetupInProgress());

  base::string16 status_label;
  base::string16 link_label;
  bool status_has_error =
      sync_ui_util::GetStatusLabels(profile_, service, *signin,
                                    sync_ui_util::WITH_HTML, &status_label,
                                    &link_label) == sync_ui_util::SYNC_ERROR;
  sync_status->SetString("statusText", status_label);
  sync_status->SetString("actionLinkText", link_label);
  sync_status->SetBoolean("hasError", status_has_error);

  sync_status->SetBoolean("managed", service && service->IsManaged());
  sync_status->SetBoolean("signedIn", signin->IsAuthenticated());
  sync_status->SetBoolean("hasUnrecoverableError",
                          service && service->HasUnrecoverableError());

  return sync_status;
}

bool PeopleHandler::IsExistingWizardPresent() {
  LoginUIService* service = GetLoginUIService();
  DCHECK(service);
  return service->current_login_ui() != nullptr;
}

bool PeopleHandler::FocusExistingWizardIfPresent() {
  if (!IsExistingWizardPresent())
    return false;

  LoginUIService* service = GetLoginUIService();
  DCHECK(service);
  service->current_login_ui()->FocusUI();
  return true;
}

void PeopleHandler::DisplayConfigureSync(bool passphrase_failed) {
  // Should never call this when we are not signed in.
  DCHECK(SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated());
  ProfileSyncService* service = GetSyncService();
  DCHECK(service);
  if (!service->IsBackendInitialized()) {
    service->RequestStart();

    // See if it's even possible to bring up the sync backend - if not
    // (unrecoverable error?), don't bother displaying a spinner that will be
    // immediately closed because this leads to some ugly infinite UI loop (see
    // http://crbug.com/244769).
    if (SyncStartupTracker::GetSyncServiceState(profile_) !=
        SyncStartupTracker::SYNC_STARTUP_ERROR) {
      DisplaySpinner();
    }

    // Start SyncSetupTracker to wait for sync to initialize.
    sync_startup_tracker_.reset(new SyncStartupTracker(profile_, this));
    return;
  }

  // Should only get here if user is signed in and sync is initialized, so no
  // longer need a SyncStartupTracker.
  sync_startup_tracker_.reset();
  configuring_sync_ = true;
  DCHECK(service->IsBackendInitialized())
      << "Cannot configure sync until the sync backend is initialized";

  // Setup args for the sync configure screen:
  //   syncAllDataTypes: true if the user wants to sync everything
  //   syncNothing: true if the user wants to sync nothing
  //   <data_type>Registered: true if the associated data type is supported
  //   <data_type>Synced: true if the user wants to sync that specific data type
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
  sync_driver::SyncPrefs sync_prefs(profile_->GetPrefs());
  args.SetBoolean("passphraseFailed", passphrase_failed);
  args.SetBoolean("syncAllDataTypes", sync_prefs.HasKeepEverythingSynced());
  args.SetBoolean("syncNothing", false);  // Always false during initial setup.
  args.SetBoolean("encryptAllData", service->IsEncryptEverythingEnabled());
  args.SetBoolean("encryptAllDataAllowed",
                  service->IsEncryptEverythingAllowed());

  // We call IsPassphraseRequired() here, instead of calling
  // IsPassphraseRequiredForDecryption(), because we want to show the passphrase
  // UI even if no encrypted data types are enabled.
  args.SetBoolean("showPassphrase", service->IsPassphraseRequired());

  // To distinguish between FROZEN_IMPLICIT_PASSPHRASE and CUSTOM_PASSPHRASE
  // we only set usePassphrase for CUSTOM_PASSPHRASE.
  args.SetBoolean("usePassphrase",
                  service->GetPassphraseType() == syncer::CUSTOM_PASSPHRASE);
  base::Time passphrase_time = service->GetExplicitPassphraseTime();
  syncer::PassphraseType passphrase_type = service->GetPassphraseType();
  if (!passphrase_time.is_null()) {
    base::string16 passphrase_time_str =
        base::TimeFormatShortDate(passphrase_time);
    args.SetString("enterPassphraseBody",
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
        args.SetString("fullEncryptionBody",
                       GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM));
        break;
    }
  } else if (passphrase_type == syncer::CUSTOM_PASSPHRASE) {
    args.SetString("fullEncryptionBody",
                   GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM));
  } else {
    args.SetString("fullEncryptionBody",
                   GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_DATA));
  }

  base::StringValue page("configure");
  web_ui()->CallJavascriptFunction("settings.SyncPrivateApi.showSyncSetupPage",
                                   page, args);

  // Make sure the tab used for the Gaia sign in does not cover the settings
  // tab.
  FocusUI();
}

LoginUIService* PeopleHandler::GetLoginUIService() const {
  return LoginUIServiceFactory::GetForProfile(profile_);
}

void PeopleHandler::UpdateSyncState() {
  web_ui()->CallJavascriptFunction("settings.SyncPrivateApi.sendSyncStatus",
                                   *GetSyncStateDictionary());
}

void PeopleHandler::OnSigninAllowedPrefChange() {
  UpdateSyncState();
}

}  // namespace settings
