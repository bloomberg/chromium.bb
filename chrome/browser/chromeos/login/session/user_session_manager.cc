// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/user_session_manager.h"

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/first_run/goodies_displayer.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/auth/chrome_cryptohome_authenticator.h"
#include "chrome/browser/chromeos/login/chrome_restart_request.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/profile_auth_data.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter_factory.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/signin/token_handle_fetcher.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/input_events_blocker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/component_updater/ev_whitelist_component_installer.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_brand_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/crl_set_fetcher.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cert_loader.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/auth/stub_authenticator.h"
#include "chromeos/login/user_names.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/component_updater/component_updater_service.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "url/gurl.h"

#if defined(ENABLE_RLZ)
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "components/rlz/rlz_tracker.h"
#endif

namespace chromeos {

namespace {

// Milliseconds until we timeout our attempt to fetch flags from the child
// account service.
static const int kFlagsFetchingLoginTimeoutMs = 1000;

// The maximum ammount of time that we are willing to delay a browser restart
// for, waiting for a session restore to finish.
static const int kMaxRestartDelaySeconds = 10;

// ChromeVox tutorial URL (used in place of "getting started" url when
// accessibility is enabled).
const char kChromeVoxTutorialURLPattern[] =
    "http://www.chromevox.com/tutorial/index.html?lang=%s";

void InitLocaleAndInputMethodsForNewUser(
    UserSessionManager* session_manager,
    Profile* profile,
    const std::string& public_session_locale,
    const std::string& public_session_input_method) {
  PrefService* prefs = profile->GetPrefs();
  std::string locale;
  if (!public_session_locale.empty()) {
    // If this is a public session and the user chose a |public_session_locale|,
    // write it to |prefs| so that the UI switches to it.
    locale = public_session_locale;
    prefs->SetString(prefs::kApplicationLocale, locale);

    // Suppress the locale change dialog.
    prefs->SetString(prefs::kApplicationLocaleAccepted, locale);
  } else {
    // Otherwise, assume that the session will use the current UI locale.
    locale = g_browser_process->GetApplicationLocale();
  }

  // First, we'll set kLanguagePreloadEngines.
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();

  input_method::InputMethodDescriptor preferred_input_method;
  if (!public_session_input_method.empty()) {
    // If this is a public session and the user chose a valid
    // |public_session_input_method|, use it as the |preferred_input_method|.
    const input_method::InputMethodDescriptor* const descriptor =
        manager->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
            public_session_input_method);
    if (descriptor) {
      preferred_input_method = *descriptor;
    } else {
      LOG(WARNING) << "Public session is initialized with an invalid IME"
                   << ", id=" << public_session_input_method;
    }
  }

  // If |preferred_input_method| is not set, use the currently active input
  // method.
  if (preferred_input_method.id().empty()) {
    preferred_input_method =
        session_manager->GetDefaultIMEState(profile)->GetCurrentInputMethod();
    const input_method::InputMethodDescriptor* descriptor =
        manager->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
            manager->GetInputMethodUtil()->GetHardwareInputMethodIds()[0]);
    // If the hardware input method's keyboard layout is the same as the
    // default input method (e.g. from GaiaScreen), use the hardware input
    // method. Note that the hardware input method can be non-login-able.
    // Refer to the issue chrome-os-partner:48623.
    if (descriptor &&
        descriptor->GetPreferredKeyboardLayout() ==
            preferred_input_method.GetPreferredKeyboardLayout()) {
      preferred_input_method = *descriptor;
    }
  }

  // Derive kLanguagePreloadEngines from |locale| and |preferred_input_method|.
  std::vector<std::string> input_method_ids;
  manager->GetInputMethodUtil()->GetFirstLoginInputMethodIds(
      locale, preferred_input_method, &input_method_ids);

  // Save the input methods in the user's preferences.
  StringPrefMember language_preload_engines;
  language_preload_engines.Init(prefs::kLanguagePreloadEngines, prefs);
  language_preload_engines.SetValue(base::JoinString(input_method_ids, ","));
  BootTimesRecorder::Get()->AddLoginTimeMarker("IMEStarted", false);

  // Second, we'll set kLanguagePreferredLanguages.
  std::vector<std::string> language_codes;

  // The current locale should be on the top.
  language_codes.push_back(locale);

  // Add input method IDs based on the input methods, as there may be
  // input methods that are unrelated to the current locale. Example: the
  // hardware keyboard layout xkb:us::eng is used for logging in, but the
  // UI language is set to French. In this case, we should set "fr,en"
  // to the preferred languages preference.
  std::vector<std::string> candidates;
  manager->GetInputMethodUtil()->GetLanguageCodesFromInputMethodIds(
      input_method_ids, &candidates);
  for (size_t i = 0; i < candidates.size(); ++i) {
    const std::string& candidate = candidates[i];
    // Skip if it's already in language_codes.
    if (std::count(language_codes.begin(), language_codes.end(),
                   candidate) == 0) {
      language_codes.push_back(candidate);
    }
  }

  // Save the preferred languages in the user's preferences.
  prefs->SetString(prefs::kLanguagePreferredLanguages,
                   base::JoinString(language_codes, ","));

  // Indicate that we need to merge the syncable input methods when we sync,
  // since we have not applied the synced prefs before.
  prefs->SetBoolean(prefs::kLanguageShouldMergeInputMethods, true);
}

#if defined(ENABLE_RLZ)
// Flag file that disables RLZ tracking, when present.
const base::FilePath::CharType kRLZDisabledFlagName[] =
    FILE_PATH_LITERAL(".rlz_disabled");

base::FilePath GetRlzDisabledFlagPath() {
  base::FilePath homedir;
  PathService::Get(base::DIR_HOME, &homedir);
  return homedir.Append(kRLZDisabledFlagName);
}
#endif

// Callback to GetNSSCertDatabaseForProfile. It starts CertLoader using the
// provided NSS database. It must be called for primary user only.
void OnGetNSSCertDatabaseForUser(net::NSSCertDatabase* database) {
  if (!CertLoader::IsInitialized())
    return;

  CertLoader::Get()->StartWithNSSDB(database);
}

// Returns new CommandLine with per-user flags.
base::CommandLine CreatePerSessionCommandLine(Profile* profile) {
  base::CommandLine user_flags(base::CommandLine::NO_PROGRAM);
  flags_ui::PrefServiceFlagsStorage flags_storage_(profile->GetPrefs());
  about_flags::ConvertFlagsToSwitches(&flags_storage_, &user_flags,
                                      flags_ui::kAddSentinels);
  return user_flags;
}

// Returns true if restart is needed to apply per-session flags.
bool NeedRestartToApplyPerSessionFlags(
    const base::CommandLine& user_flags,
    std::set<base::CommandLine::StringType>* out_command_line_difference) {
  // Don't restart browser if it is not first profile in session.
  if (user_manager::UserManager::Get()->GetLoggedInUsers().size() != 1)
    return false;

  // Only restart if needed and if not going into managed mode.
  if (user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser())
    return false;

  if (about_flags::AreSwitchesIdenticalToCurrentCommandLine(
          user_flags, *base::CommandLine::ForCurrentProcess(),
          out_command_line_difference)) {
    return false;
  }

  return true;
}

bool CanPerformEarlyRestart() {
  // Desktop build is used for development only. Early restart is not supported.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return false;

  if (!ChromeUserManager::Get()
           ->GetCurrentUserFlow()
           ->SupportsEarlyRestartToApplyFlags()) {
    return false;
  }

  const ExistingUserController* controller =
      ExistingUserController::current_controller();
  if (!controller)
    return true;

  // Early restart is possible only if OAuth token is up to date.

  if (controller->password_changed())
    return false;

  if (controller->auth_mode() != LoginPerformer::AUTH_MODE_INTERNAL)
    return false;

  // No early restart if Easy unlock key needs to be updated.
  if (UserSessionManager::GetInstance()->NeedsToUpdateEasyUnlockKeys())
    return false;

  return true;
}

void LogCustomSwitches(const std::set<std::string>& switches) {
  if (!VLOG_IS_ON(1))
    return;
  for (std::set<std::string>::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    VLOG(1) << "Switch leading to restart: '" << *it << "'";
  }
}

void RestartOnTimeout() {
  LOG(WARNING) << "Restarting Chrome because the time out was reached."
                  "The session restore has not finished.";
  chrome::AttemptRestart();
}

}  // namespace

UserSessionManagerDelegate::~UserSessionManagerDelegate() {
}

void UserSessionStateObserver::PendingUserSessionsRestoreFinished() {
}

UserSessionStateObserver::~UserSessionStateObserver() {
}

// static
UserSessionManager* UserSessionManager::GetInstance() {
  return base::Singleton<UserSessionManager, base::DefaultSingletonTraits<
                                                 UserSessionManager>>::get();
}

// static
void UserSessionManager::OverrideHomedir() {
  // Override user homedir, check for ProfileManager being initialized as
  // it may not exist in unit tests.
  if (g_browser_process->profile_manager()) {
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    if (user_manager->GetLoggedInUsers().size() == 1) {
      base::FilePath homedir = ProfileHelper::GetProfilePathByUserIdHash(
          user_manager->GetPrimaryUser()->username_hash());
      // This path has been either created by cryptohome (on real Chrome OS
      // device) or by ProfileManager (on chromeos=1 desktop builds).
      PathService::OverrideAndCreateIfNeeded(base::DIR_HOME,
                                             homedir,
                                             true /* path is absolute */,
                                             false /* don't create */);
    }
  }
}

// static
void UserSessionManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kRLZBrand, std::string());
  registry->RegisterBooleanPref(prefs::kRLZDisabled, false);
  registry->RegisterBooleanPref(prefs::kCanShowOobeGoodiesPage, true);
}

UserSessionManager::UserSessionManager()
    : delegate_(nullptr),
      authenticator_(nullptr),
      has_auth_cookies_(false),
      user_sessions_restored_(false),
      user_sessions_restore_in_progress_(false),
      exit_after_session_restore_(false),
      session_restore_strategy_(
          OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN),
      running_easy_unlock_key_ops_(false),
      should_obtain_handles_(true),
      should_launch_browser_(true),
      waiting_for_child_account_status_(false),
      weak_factory_(this) {
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
}

UserSessionManager::~UserSessionManager() {
  // UserManager is destroyed before singletons, so we need to check if it
  // still exists.
  // TODO(nkostylev): fix order of destruction of UserManager
  // / UserSessionManager objects.
  if (user_manager::UserManager::IsInitialized())
    user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void UserSessionManager::SetShouldObtainHandleInTests(
    bool should_obtain_handles) {
  should_obtain_handles_ = should_obtain_handles;
  if (!should_obtain_handles_) {
    token_handle_fetcher_.reset();
  }
}

void UserSessionManager::CompleteGuestSessionLogin(const GURL& start_url) {
  VLOG(1) << "Completing guest session login";

  // For guest session we ask session_manager to restart Chrome with --bwsi
  // flag. We keep only some of the arguments of this process.
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine command_line(browser_command_line.GetProgram());
  GetOffTheRecordCommandLine(start_url, StartupUtils::IsOobeCompleted(),
                             browser_command_line, &command_line);

  // This makes sure that Chrome restarts with no per-session flags. The guest
  // profile will always have empty set of per-session flags. If this is not
  // done and device owner has some per-session flags, when Chrome is relaunched
  // the guest profile session flags will not match the current command line and
  // another restart will be attempted in order to reset the user flags for the
  // guest user.
  const base::CommandLine user_flags(base::CommandLine::NO_PROGRAM);
  if (!about_flags::AreSwitchesIdenticalToCurrentCommandLine(
          user_flags, *base::CommandLine::ForCurrentProcess(), NULL)) {
    DBusThreadManager::Get()->GetSessionManagerClient()->SetFlagsForUser(
        login::GuestAccountId().GetUserEmail(),
        base::CommandLine::StringVector());
  }

  RestartChrome(command_line);
}

scoped_refptr<Authenticator> UserSessionManager::CreateAuthenticator(
    AuthStatusConsumer* consumer) {
  // Screen locker needs new Authenticator instance each time.
  if (ScreenLocker::default_screen_locker()) {
    if (authenticator_.get())
      authenticator_->SetConsumer(NULL);
    authenticator_ = NULL;
  }

  if (authenticator_.get() == NULL) {
    if (injected_user_context_) {
      authenticator_ =
          new StubAuthenticator(consumer, *injected_user_context_.get());
    } else {
      authenticator_ = new ChromeCryptohomeAuthenticator(consumer);
    }
  } else {
    // TODO(nkostylev): Fix this hack by improving Authenticator dependencies.
    authenticator_->SetConsumer(consumer);
  }
  return authenticator_;
}

void UserSessionManager::StartSession(
    const UserContext& user_context,
    StartSessionType start_session_type,
    bool has_auth_cookies,
    bool has_active_session,
    UserSessionManagerDelegate* delegate) {
  delegate_ = delegate;
  start_session_type_ = start_session_type;

  VLOG(1) << "Starting session for "
          << user_context.GetAccountId().GetUserEmail();

  PreStartSession();
  CreateUserSession(user_context, has_auth_cookies);

  if (!has_active_session)
    StartCrosSession();

  // TODO(nkostylev): Notify UserLoggedIn() after profile is actually
  // ready to be used (http://crbug.com/361528).
  NotifyUserLoggedIn();

  if (!user_context.GetDeviceId().empty()) {
    user_manager::known_user::SetDeviceId(user_context.GetAccountId(),
                                          user_context.GetDeviceId());
  }

  PrepareProfile();
}

void UserSessionManager::DelegateDeleted(UserSessionManagerDelegate* delegate) {
  if (delegate_ == delegate)
    delegate_ = nullptr;
}

void UserSessionManager::PerformPostUserLoggedInActions() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager->GetLoggedInUsers().size() == 1) {
    if (network_portal_detector::IsInitialized()) {
      network_portal_detector::GetInstance()->SetStrategy(
          PortalDetectorStrategy::STRATEGY_ID_SESSION);
    }
  }
}

void UserSessionManager::RestoreAuthenticationSession(Profile* user_profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  // We need to restore session only for logged in GAIA (regular) users.
  // Note: stub user is a special case that is used for tests, running
  // linux_chromeos build on dev workstations w/o user_id parameters.
  // Stub user is considered to be a regular GAIA user but it has special
  // user_id (kStubUser) and certain services like restoring OAuth session are
  // explicitly disabled for it.
  if (!user_manager->IsUserLoggedIn() ||
      !user_manager->IsLoggedInAsUserWithGaiaAccount() ||
      user_manager->IsLoggedInAsStub()) {
    return;
  }

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(user_profile);
  DCHECK(user);
  if (!net::NetworkChangeNotifier::IsOffline()) {
    pending_signin_restore_sessions_.erase(user->email());
    RestoreAuthSessionImpl(user_profile, false /* has_auth_cookies */);
  } else {
    // Even if we're online we should wait till initial
    // OnConnectionTypeChanged() call. Otherwise starting fetchers too early may
    // end up canceling all request when initial network connection type is
    // processed. See http://crbug.com/121643.
    pending_signin_restore_sessions_.insert(user->email());
  }
}

void UserSessionManager::RestoreActiveSessions() {
  user_sessions_restore_in_progress_ = true;
  DBusThreadManager::Get()->GetSessionManagerClient()->RetrieveActiveSessions(
      base::Bind(&UserSessionManager::OnRestoreActiveSessions, AsWeakPtr()));
}

bool UserSessionManager::UserSessionsRestored() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return user_sessions_restored_;
}

bool UserSessionManager::UserSessionsRestoreInProgress() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return user_sessions_restore_in_progress_;
}

void UserSessionManager::InitRlz(Profile* profile) {
#if defined(ENABLE_RLZ)
  if (!g_browser_process->local_state()->HasPrefPath(prefs::kRLZBrand)) {
    // Read brand code asynchronously from an OEM data and repost ourselves.
    google_brand::chromeos::InitBrand(
        base::Bind(&UserSessionManager::InitRlz, AsWeakPtr(), profile));
    return;
  }
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(false).get(),
      FROM_HERE,
      base::Bind(&base::PathExists, GetRlzDisabledFlagPath()),
      base::Bind(&UserSessionManager::InitRlzImpl, AsWeakPtr(), profile));
#endif
}

void UserSessionManager::SetFirstLoginPrefs(
    Profile* profile,
    const std::string& public_session_locale,
    const std::string& public_session_input_method) {
  VLOG(1) << "Setting first login prefs";
  InitLocaleAndInputMethodsForNewUser(
      this, profile, public_session_locale, public_session_input_method);
}

bool UserSessionManager::GetAppModeChromeClientOAuthInfo(
    std::string* chrome_client_id, std::string* chrome_client_secret) {
  if (!chrome::IsRunningInForcedAppMode() ||
      chrome_client_id_.empty() ||
      chrome_client_secret_.empty()) {
    return false;
  }

  *chrome_client_id = chrome_client_id_;
  *chrome_client_secret = chrome_client_secret_;
  return true;
}

void UserSessionManager::SetAppModeChromeClientOAuthInfo(
    const std::string& chrome_client_id,
    const std::string& chrome_client_secret) {
  if (!chrome::IsRunningInForcedAppMode())
    return;

  chrome_client_id_ = chrome_client_id;
  chrome_client_secret_ = chrome_client_secret;
}

void UserSessionManager::DoBrowserLaunch(Profile* profile,
                                         LoginDisplayHost* login_host) {
  DoBrowserLaunchInternal(profile, login_host, false /* locale_pref_checked */);
}

bool UserSessionManager::RespectLocalePreference(
    Profile* profile,
    const user_manager::User* user,
    const locale_util::SwitchLanguageCallback& callback) const {
  // TODO(alemate): http://crbug.com/288941 : Respect preferred language list in
  // the Google user profile.
  if (g_browser_process == NULL)
    return false;

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user || (user_manager->IsUserLoggedIn() &&
                user != user_manager->GetPrimaryUser())) {
    return false;
  }

  // In case of multi-profiles session we don't apply profile locale
  // because it is unsafe.
  if (user_manager->GetLoggedInUsers().size() != 1)
    return false;

  const PrefService* prefs = profile->GetPrefs();
  if (prefs == NULL)
    return false;

  std::string pref_locale;
  const std::string pref_app_locale =
      prefs->GetString(prefs::kApplicationLocale);
  const std::string pref_bkup_locale =
      prefs->GetString(prefs::kApplicationLocaleBackup);

  pref_locale = pref_app_locale;
  if (pref_locale.empty())
    pref_locale = pref_bkup_locale;

  const std::string* account_locale = NULL;
  if (pref_locale.empty() && user->has_gaia_account()) {
    if (user->GetAccountLocale() == NULL)
      return false;  // wait until Account profile is loaded.
    account_locale = user->GetAccountLocale();
    pref_locale = *account_locale;
  }
  const std::string global_app_locale =
      g_browser_process->GetApplicationLocale();
  if (pref_locale.empty())
    pref_locale = global_app_locale;
  DCHECK(!pref_locale.empty());
  VLOG(1) << "RespectLocalePreference: "
          << "app_locale='" << pref_app_locale << "', "
          << "bkup_locale='" << pref_bkup_locale << "', "
          << (account_locale != NULL
              ? (std::string("account_locale='") + (*account_locale) +
                 "'. ")
              : (std::string("account_locale - unused. ")))
          << " Selected '" << pref_locale << "'";
  profile->ChangeAppLocale(
      pref_locale,
      user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT ?
          Profile::APP_LOCALE_CHANGED_VIA_PUBLIC_SESSION_LOGIN :
          Profile::APP_LOCALE_CHANGED_VIA_LOGIN);

  // Here we don't enable keyboard layouts for normal users. Input methods
  // are set up when the user first logs in. Then the user may customize the
  // input methods.  Hence changing input methods here, just because the user's
  // UI language is different from the login screen UI language, is not
  // desirable. Note that input method preferences are synced, so users can use
  // their farovite input methods as soon as the preferences are synced.
  //
  // For Guest mode, user locale preferences will never get initialized.
  // So input methods should be enabled somewhere.
  const bool enable_layouts =
      user_manager::UserManager::Get()->IsLoggedInAsGuest();
  locale_util::SwitchLanguage(pref_locale, enable_layouts,
                              false /* login_layouts_only */, callback,
                              profile);

  return true;
}

bool UserSessionManager::RestartToApplyPerSessionFlagsIfNeed(
    Profile* profile,
    bool early_restart) {
  if (ProfileHelper::IsSigninProfile(profile))
    return false;

  if (early_restart && !CanPerformEarlyRestart())
    return false;

  // We can't really restart if we've already restarted as a part of
  // user session restore after crash of in case when flags were changed inside
  // user session.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kLoginUser))
    return false;

  // We can't restart if that's a second user sign in that is happening.
  if (user_manager::UserManager::Get()->GetLoggedInUsers().size() > 1)
    return false;

  const base::CommandLine user_flags(CreatePerSessionCommandLine(profile));
  std::set<base::CommandLine::StringType> command_line_difference;
  if (!NeedRestartToApplyPerSessionFlags(user_flags, &command_line_difference))
    return false;

  LogCustomSwitches(command_line_difference);

  about_flags::ReportCustomFlags("Login.CustomFlags", command_line_difference);

  base::CommandLine::StringVector flags;
  // argv[0] is the program name |base::CommandLine::NO_PROGRAM|.
  flags.assign(user_flags.argv().begin() + 1, user_flags.argv().end());
  LOG(WARNING) << "Restarting to apply per-session flags...";
  DBusThreadManager::Get()->GetSessionManagerClient()->SetFlagsForUser(
      user_manager::UserManager::Get()->GetActiveUser()->email(), flags);
  AttemptRestart(profile);
  return true;
}

bool UserSessionManager::NeedsToUpdateEasyUnlockKeys() const {
  return user_context_.GetAccountId().is_valid() &&
         user_manager::User::TypeHasGaiaAccount(user_context_.GetUserType()) &&
         user_context_.GetKey() && !user_context_.GetKey()->GetSecret().empty();
}

bool UserSessionManager::CheckEasyUnlockKeyOps(const base::Closure& callback) {
  if (!running_easy_unlock_key_ops_)
    return false;

  // Assumes only one deferred callback is needed.
  DCHECK(easy_unlock_key_ops_finished_callback_.is_null());

  easy_unlock_key_ops_finished_callback_ = callback;
  return true;
}

void UserSessionManager::AddSessionStateObserver(
    chromeos::UserSessionStateObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  session_state_observer_list_.AddObserver(observer);
}

void UserSessionManager::RemoveSessionStateObserver(
    chromeos::UserSessionStateObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  session_state_observer_list_.RemoveObserver(observer);
}

void UserSessionManager::OnSessionRestoreStateChanged(
    Profile* user_profile,
    OAuth2LoginManager::SessionRestoreState state) {
  user_manager::User::OAuthTokenStatus user_status =
      user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN;
  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);

  bool connection_error = false;
  switch (state) {
    case OAuth2LoginManager::SESSION_RESTORE_DONE:
      user_status = user_manager::User::OAUTH2_TOKEN_STATUS_VALID;
      break;
    case OAuth2LoginManager::SESSION_RESTORE_FAILED:
      user_status = user_manager::User::OAUTH2_TOKEN_STATUS_INVALID;
      break;
    case OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED:
      connection_error = true;
      break;
    case OAuth2LoginManager::SESSION_RESTORE_NOT_STARTED:
    case OAuth2LoginManager::SESSION_RESTORE_PREPARING:
    case OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS:
      return;
  }

  // We should not be clearing existing token state if that was a connection
  // error. http://crbug.com/295245
  if (!connection_error) {
    // We are in one of "done" states here.
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        user_manager::UserManager::Get()->GetLoggedInUser()->GetAccountId(),
        user_status);
  }

  login_manager->RemoveObserver(this);

  if (exit_after_session_restore_ &&
      (state  == OAuth2LoginManager::SESSION_RESTORE_DONE ||
       state  == OAuth2LoginManager::SESSION_RESTORE_FAILED ||
       state  == OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED)) {
    LOG(WARNING) << "Restarting Chrome after session restore finishes, "
                 << "most likely due to custom flags.";

    // We need to restart cleanly in this case to make sure OAuth2 RT is
    // actually saved.
    chrome::AttemptRestart();
  }
}

void UserSessionManager::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  bool is_running_test =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestName) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(::switches::kTestType);
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE ||
      !user_manager->IsUserLoggedIn() ||
      !user_manager->IsLoggedInAsUserWithGaiaAccount() ||
      user_manager->IsLoggedInAsStub() || is_running_test) {
    return;
  }

  // Need to iterate over all users and their OAuth2 session state.
  const user_manager::UserList& users = user_manager->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end();
       ++it) {
    if (!(*it)->is_profile_created())
      continue;

    Profile* user_profile = ProfileHelper::Get()->GetProfileByUserUnsafe(*it);
    bool should_restore_session =
        pending_signin_restore_sessions_.find((*it)->email()) !=
        pending_signin_restore_sessions_.end();
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);
    if (login_manager->SessionRestoreIsRunning()) {
      // If we come online for the first time after successful offline login,
      // we need to kick off OAuth token verification process again.
      login_manager->ContinueSessionRestore();
    } else if (should_restore_session) {
      pending_signin_restore_sessions_.erase((*it)->email());
      RestoreAuthSessionImpl(user_profile, false /* has_auth_cookies */);
    }
  }
}

void UserSessionManager::OnProfilePrepared(Profile* profile,
                                           bool browser_launched) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestName)) {
    // Did not log in (we crashed or are debugging), need to restore Sync.
    // TODO(nkostylev): Make sure that OAuth state is restored correctly for all
    // users once it is fully multi-profile aware. http://crbug.com/238987
    // For now if we have other user pending sessions they'll override OAuth
    // session restore for previous users.
    RestoreAuthenticationSession(profile);
  }

  // Restore other user sessions if any.
  RestorePendingUserSessions();
}

void UserSessionManager::ChildAccountStatusReceivedCallback(Profile* profile) {
  StopChildStatusObserving(profile);
}

void UserSessionManager::StopChildStatusObserving(Profile* profile) {
  if (waiting_for_child_account_status_ &&
      !SessionStartupPref::TypeIsManaged(profile->GetPrefs())) {
    InitializeStartUrls();
  }
  waiting_for_child_account_status_ = false;
}

void UserSessionManager::CreateUserSession(const UserContext& user_context,
                                           bool has_auth_cookies) {
  user_context_ = user_context;
  has_auth_cookies_ = has_auth_cookies;
  InitSessionRestoreStrategy();
  StoreUserContextDataBeforeProfileIsCreated();
}

void UserSessionManager::PreStartSession() {
  // Switch log file as soon as possible.
  if (base::SysInfo::IsRunningOnChromeOS())
    logging::RedirectChromeLogging(*(base::CommandLine::ForCurrentProcess()));
}

void UserSessionManager::StoreUserContextDataBeforeProfileIsCreated() {
  // Store obfuscated GAIA ID.
  if (!user_context_.GetGaiaID().empty()) {
    user_manager::known_user::UpdateGaiaID(user_context_.GetAccountId(),
                                           user_context_.GetGaiaID());
  }
}

void UserSessionManager::StartCrosSession() {
  BootTimesRecorder* btl = BootTimesRecorder::Get();
  btl->AddLoginTimeMarker("StartSession-Start", false);
  DBusThreadManager::Get()->GetSessionManagerClient()->StartSession(
      user_context_.GetAccountId().GetUserEmail());
  btl->AddLoginTimeMarker("StartSession-End", false);
}

void UserSessionManager::NotifyUserLoggedIn() {
  BootTimesRecorder* btl = BootTimesRecorder::Get();
  btl->AddLoginTimeMarker("UserLoggedIn-Start", false);
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  user_manager->UserLoggedIn(user_context_.GetAccountId(),
                             user_context_.GetUserIDHash(), false);
  btl->AddLoginTimeMarker("UserLoggedIn-End", false);
}

void UserSessionManager::PrepareProfile() {
  const bool is_demo_session = DemoAppLauncher::IsDemoAppSession(
      user_context_.GetAccountId().GetUserEmail());

  // TODO(nkostylev): Figure out whether demo session is using the right profile
  // path or not. See https://codereview.chromium.org/171423009
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileHelper::GetProfilePathByUserIdHash(user_context_.GetUserIDHash()),
      base::Bind(&UserSessionManager::OnProfileCreated, AsWeakPtr(),
                 user_context_, is_demo_session),
      base::string16(), std::string(), std::string());
}

void UserSessionManager::OnProfileCreated(const UserContext& user_context,
                                          bool is_incognito_profile,
                                          Profile* profile,
                                          Profile::CreateStatus status) {
  CHECK(profile);

  switch (status) {
    case Profile::CREATE_STATUS_CREATED:
      // Profile created but before initializing extensions and promo resources.
      InitProfilePreferences(profile, user_context);
      break;
    case Profile::CREATE_STATUS_INITIALIZED:
      // Profile is created, extensions and promo resources are initialized.
      // At this point all other Chrome OS services will be notified that it is
      // safe to use this profile.
      UserProfileInitialized(profile, is_incognito_profile,
                             user_context.GetAccountId());
      break;
    case Profile::CREATE_STATUS_LOCAL_FAIL:
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::CREATE_STATUS_CANCELED:
    case Profile::MAX_CREATE_STATUS:
      NOTREACHED();
      break;
  }
}

void UserSessionManager::InitProfilePreferences(
    Profile* profile,
    const UserContext& user_context) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (user->GetType() == user_manager::USER_TYPE_KIOSK_APP &&
      profile->IsNewProfile()) {
    ChromeUserManager::Get()->SetIsCurrentUserNew(true);
  }

  if (user->is_active()) {
    input_method::InputMethodManager* manager =
        input_method::InputMethodManager::Get();
    manager->SetState(GetDefaultIMEState(profile));
  }
  if (user_manager::UserManager::Get()->IsCurrentUserNew()) {
    SetFirstLoginPrefs(profile,
                       user_context.GetPublicSessionLocale(),
                       user_context.GetPublicSessionInputMethod());
  }

  if (user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser()) {
    user_manager::User* active_user =
        user_manager::UserManager::Get()->GetActiveUser();
    std::string supervised_user_sync_id =
        ChromeUserManager::Get()->GetSupervisedUserManager()->GetUserSyncId(
            active_user->GetAccountId().GetUserEmail());
    profile->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                   supervised_user_sync_id);
  } else if (user_manager::UserManager::Get()->
      IsLoggedInAsUserWithGaiaAccount()) {
    // Get the Gaia ID from the user context.  If it's not available, this may
    // not be available when unlocking a previously opened profile, or when
    // creating a supervised users.  However, in these cases the gaia_id should
    // be already available in the account tracker.
    std::string gaia_id = user_context.GetGaiaID();
    if (gaia_id.empty()) {
      AccountTrackerService* account_tracker =
          AccountTrackerServiceFactory::GetForProfile(profile);
      const AccountInfo info = account_tracker->FindAccountInfoByEmail(
          user_context.GetAccountId().GetUserEmail());
      gaia_id = info.gaia;
      DCHECK(!gaia_id.empty());
    }

    // Make sure that the google service username is properly set (we do this
    // on every sign in, not just the first login, to deal with existing
    // profiles that might not have it set yet).
    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfile(profile);
    signin_manager->SetAuthenticatedAccountInfo(
        gaia_id, user_context.GetAccountId().GetUserEmail());

    // Backfill GAIA ID in user prefs stored in Local State.
    std::string tmp_gaia_id;
    if (!user_manager::known_user::FindGaiaID(user_context.GetAccountId(),
                                              &tmp_gaia_id) &&
        !gaia_id.empty()) {
      user_manager::known_user::UpdateGaiaID(user_context.GetAccountId(),
                                             gaia_id);
    }
  }
}

void UserSessionManager::UserProfileInitialized(Profile* profile,
                                                bool is_incognito_profile,
                                                const AccountId& account_id) {
  // Demo user signed in.
  if (is_incognito_profile) {
    profile->OnLogin();

    // Send the notification before creating the browser so additional objects
    // that need the profile (e.g. the launcher) can be created first.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
        content::NotificationService::AllSources(),
        content::Details<Profile>(profile));

    if (delegate_)
      delegate_->OnProfilePrepared(profile, false);

    return;
  }

  BootTimesRecorder* btl = BootTimesRecorder::Get();
  btl->AddLoginTimeMarker("UserProfileGotten", false);

  if (user_context_.IsUsingOAuth()) {
    // Retrieve the policy that indicates whether to continue copying
    // authentication cookies set by a SAML IdP on subsequent logins after the
    // first.
    bool transfer_saml_auth_cookies_on_subsequent_login = false;
    if (has_auth_cookies_) {
      const user_manager::User* user =
          user_manager::UserManager::Get()->FindUser(account_id);
      if (user->is_affiliated()) {
        CrosSettings::Get()->GetBoolean(
            kAccountsPrefTransferSAMLCookies,
            &transfer_saml_auth_cookies_on_subsequent_login);
      }
    }

    // Transfers authentication-related data from the profile that was used for
    // authentication to the user's profile. The proxy authentication state is
    // transferred unconditionally. If the user authenticated via an auth
    // extension, authentication cookies and channel IDs will be transferred as
    // well when the user's cookie jar is empty. If the cookie jar is not empty,
    // the authentication states in the browser context and the user's profile
    // must be merged using /MergeSession instead. Authentication cookies set by
    // a SAML IdP will also be transferred when the user's cookie jar is not
    // empty if |transfer_saml_auth_cookies_on_subsequent_login| is true.
    const bool transfer_auth_cookies_and_channel_ids_on_first_login =
        has_auth_cookies_;

    net::URLRequestContextGetter* auth_request_context =
        GetAuthRequestContext();

    // Authentication request context may be missing especially if user didn't
    // sign in using GAIA (webview) and webview didn't yet initialize.
    if (auth_request_context) {
      ProfileAuthData::Transfer(
          auth_request_context, profile->GetRequestContext(),
          transfer_auth_cookies_and_channel_ids_on_first_login,
          transfer_saml_auth_cookies_on_subsequent_login,
          base::Bind(
              &UserSessionManager::CompleteProfileCreateAfterAuthTransfer,
              AsWeakPtr(), profile));
    } else {
      // We need to post task so that OnProfileCreated() caller sends out
      // NOTIFICATION_PROFILE_CREATED which marks user profile as initialized.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(
              &UserSessionManager::CompleteProfileCreateAfterAuthTransfer,
              AsWeakPtr(), profile));
    }
    return;
  }

  FinalizePrepareProfile(profile);
}

void UserSessionManager::CompleteProfileCreateAfterAuthTransfer(
    Profile* profile) {
  RestoreAuthSessionImpl(profile, has_auth_cookies_);
  FinalizePrepareProfile(profile);
}

void UserSessionManager::FinalizePrepareProfile(Profile* profile) {
  BootTimesRecorder* btl = BootTimesRecorder::Get();

  // Own TPM device if, for any reason, it has not been done in EULA screen.
  CryptohomeClient* client = DBusThreadManager::Get()->GetCryptohomeClient();
  btl->AddLoginTimeMarker("TPMOwn-Start", false);
  if (cryptohome_util::TpmIsEnabled() && !cryptohome_util::TpmIsBeingOwned()) {
    if (cryptohome_util::TpmIsOwned())
      client->CallTpmClearStoredPasswordAndBlock();
    else
      client->TpmCanAttemptOwnership(EmptyVoidDBusMethodCallback());
  }
  btl->AddLoginTimeMarker("TPMOwn-End", false);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager->IsLoggedInAsUserWithGaiaAccount()) {
    if (user_context_.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITH_SAML)
      user_manager::known_user::UpdateUsingSAML(user_context_.GetAccountId(),
                                                true);
    SAMLOfflineSigninLimiter* saml_offline_signin_limiter =
        SAMLOfflineSigninLimiterFactory::GetForProfile(profile);
    if (saml_offline_signin_limiter)
      saml_offline_signin_limiter->SignedIn(user_context_.GetAuthFlow());
  }

  profile->OnLogin();

  g_browser_process->platform_part()->SessionManager()->SetSessionState(
      session_manager::SESSION_STATE_LOGGED_IN_NOT_ACTIVE);

  // Send the notification before creating the browser so additional objects
  // that need the profile (e.g. the launcher) can be created first.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(profile));

  // Initialize various services only for primary user.
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (user_manager->GetPrimaryUser() == user) {
    InitRlz(profile);
    InitializeCerts(profile);
    InitializeCRLSetFetcher(user);
    InitializeEVCertificatesWhitelistComponent(user);

    if (arc::ArcBridgeService::GetEnabled(
            base::CommandLine::ForCurrentProcess())) {
      DCHECK(arc::ArcServiceManager::Get());
      arc::ArcServiceManager::Get()->OnPrimaryUserProfilePrepared(
          multi_user_util::GetAccountIdFromProfile(profile));
      arc::ArcAuthService::Get()->OnPrimaryUserProfilePrepared(profile);
    }
  }

  UpdateEasyUnlockKeys(user_context_);
  user_context_.ClearSecrets();
  if (TokenHandlesEnabled()) {
    CreateTokenUtilIfMissing();
    if (token_handle_util_->ShouldObtainHandle(user->GetAccountId())) {
      if (!token_handle_fetcher_.get()) {
        token_handle_fetcher_.reset(new TokenHandleFetcher(
            token_handle_util_.get(), user->GetAccountId()));
        token_handle_fetcher_->BackfillToken(
            profile, base::Bind(&UserSessionManager::OnTokenHandleObtained,
                                weak_factory_.GetWeakPtr()));
      }
    }
  }

  // Now that profile is ready, proceed to either alternative login flows or
  // launch browser.
  bool browser_launched = InitializeUserSession(profile);

  // If needed, create browser observer to display first run OOBE Goodies page.
  first_run::GoodiesDisplayer::Init();

  // TODO(nkostylev): This pointer should probably never be NULL, but it looks
  // like OnProfileCreated() may be getting called before
  // UserSessionManager::PrepareProfile() has set |delegate_| when Chrome is
  // killed during shutdown in tests -- see http://crosbug.com/18269.  Replace
  // this 'if' statement with a CHECK(delegate_) once the underlying issue is
  // resolved.
  if (delegate_)
    delegate_->OnProfilePrepared(profile, browser_launched);
}

void UserSessionManager::ActivateWizard(const std::string& screen_name) {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  CHECK(host);
  host->StartWizard(screen_name);
}

void UserSessionManager::InitializeStartUrls() const {
  // Child account status should be known by the time of this call.
  std::vector<std::string> start_urls;

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  bool can_show_getstarted_guide = user_manager->GetActiveUser()->GetType() ==
                                   user_manager::USER_TYPE_REGULAR;

  // Skip the default first-run behavior for public accounts.
  if (!user_manager->IsLoggedInAsPublicAccount()) {
    if (AccessibilityManager::Get()->IsSpokenFeedbackEnabled()) {
      const char* url = kChromeVoxTutorialURLPattern;
      PrefService* prefs = g_browser_process->local_state();
      const std::string current_locale =
          base::ToLowerASCII(prefs->GetString(prefs::kApplicationLocale));
      std::string vox_url = base::StringPrintf(url, current_locale.c_str());
      start_urls.push_back(vox_url);
      can_show_getstarted_guide = false;
    }
  }

  // Only show getting started guide for a new user.
  const bool should_show_getstarted_guide = user_manager->IsCurrentUserNew();

  if (can_show_getstarted_guide && should_show_getstarted_guide) {
    // Don't open default Chrome window if we're going to launch the first-run
    // app. Because we don't want the first-run app to be hidden in the
    // background.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kSilentLaunch);
    first_run::MaybeLaunchDialogAfterSessionStart();
  } else {
    for (size_t i = 0; i < start_urls.size(); ++i) {
      base::CommandLine::ForCurrentProcess()->AppendArg(start_urls[i]);
    }
  }
}

bool UserSessionManager::InitializeUserSession(Profile* profile) {
  ChildAccountService* child_service =
      ChildAccountServiceFactory::GetForProfile(profile);
  child_service->AddChildStatusReceivedCallback(
      base::Bind(&UserSessionManager::ChildAccountStatusReceivedCallback,
                 weak_factory_.GetWeakPtr(), profile));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&UserSessionManager::StopChildStatusObserving,
                            weak_factory_.GetWeakPtr(), profile),
      base::TimeDelta::FromMilliseconds(kFlagsFetchingLoginTimeoutMs));

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Kiosk apps has their own session initialization pipeline.
  if (user_manager->IsLoggedInAsKioskApp())
    return false;

  if (start_session_type_ == PRIMARY_USER_SESSION) {
    UserFlow* user_flow = ChromeUserManager::Get()->GetCurrentUserFlow();
    WizardController* oobe_controller = WizardController::default_controller();
    base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
    bool skip_post_login_screens =
        user_flow->ShouldSkipPostLoginScreens() ||
        (oobe_controller && oobe_controller->skip_post_login_screens()) ||
        cmdline->HasSwitch(chromeos::switches::kOobeSkipPostLogin);

    if (user_manager->IsCurrentUserNew() && !skip_post_login_screens) {
      // Don't specify start URLs if the administrator has configured the start
      // URLs via policy.
      if (!SessionStartupPref::TypeIsManaged(profile->GetPrefs())) {
        if (child_service->IsChildAccountStatusKnown())
          InitializeStartUrls();
        else
          waiting_for_child_account_status_ = true;
      }

      // Mark the device as registered., i.e. the second part of OOBE as
      // completed.
      if (!StartupUtils::IsDeviceRegistered())
        StartupUtils::MarkDeviceRegistered(base::Closure());

      ActivateWizard(WizardController::kTermsOfServiceScreenName);
      return false;
    }
  }

  DoBrowserLaunch(profile, LoginDisplayHost::default_host());
  return true;
}

void UserSessionManager::InitSessionRestoreStrategy() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool in_app_mode = chrome::IsRunningInForcedAppMode();

  // Are we in kiosk app mode?
  if (in_app_mode) {
    if (command_line->HasSwitch(::switches::kAppModeOAuth2Token)) {
      user_context_.SetRefreshToken(command_line->GetSwitchValueASCII(
          ::switches::kAppModeOAuth2Token));
    }

    if (command_line->HasSwitch(::switches::kAppModeAuthCode)) {
      user_context_.SetAuthCode(command_line->GetSwitchValueASCII(
          ::switches::kAppModeAuthCode));
    }

    DCHECK(!has_auth_cookies_);
  }

  if (has_auth_cookies_) {
    session_restore_strategy_ = OAuth2LoginManager::RESTORE_FROM_COOKIE_JAR;
  } else if (!user_context_.GetRefreshToken().empty()) {
    session_restore_strategy_ =
        OAuth2LoginManager::RESTORE_FROM_PASSED_OAUTH2_REFRESH_TOKEN;
  } else {
    session_restore_strategy_ =
        OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN;
  }
}

void UserSessionManager::RestoreAuthSessionImpl(
    Profile* profile,
    bool restore_from_auth_cookies) {
  CHECK((authenticator_.get() && authenticator_->authentication_context()) ||
        !restore_from_auth_cookies);

  if (chrome::IsRunningInForcedAppMode() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableGaiaServices)) {
    return;
  }

  exit_after_session_restore_ = false;

  // Remove legacy OAuth1 token if we have one. If it's valid, we should already
  // have OAuth2 refresh token in OAuth2TokenService that could be used to
  // retrieve all other tokens and user_context.
  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile);
  login_manager->AddObserver(this);

  net::URLRequestContextGetter* auth_request_context = GetAuthRequestContext();

  // Authentication request context may not be available if user was not
  // signing in with GAIA webview (i.e. webview instance hasn't been
  // initialized at all). Use fallback request context if authenticator was
  // provided.
  // Authenticator instance may not be initialized for session
  // restore case when Chrome is restarting after crash or to apply custom user
  // flags. In that case auth_request_context will be nullptr which is accepted
  // by RestoreSession() for session restore case.
  if (!auth_request_context &&
      (authenticator_.get() && authenticator_->authentication_context())) {
    auth_request_context =
        authenticator_->authentication_context()->GetRequestContext();
  }
  login_manager->RestoreSession(auth_request_context, session_restore_strategy_,
                                user_context_.GetRefreshToken(),
                                user_context_.GetAccessToken());
}

void UserSessionManager::InitRlzImpl(Profile* profile, bool disabled) {
#if defined(ENABLE_RLZ)
  PrefService* local_state = g_browser_process->local_state();
  if (disabled) {
    // Empty brand code means an organic install (no RLZ pings are sent).
    google_brand::chromeos::ClearBrandForCurrentSession();
  }
  if (disabled != local_state->GetBoolean(prefs::kRLZDisabled)) {
    // When switching to RLZ enabled/disabled state, clear all recorded events.
    rlz::RLZTracker::ClearRlzState();
    local_state->SetBoolean(prefs::kRLZDisabled, disabled);
  }
  // Init the RLZ library.
  int ping_delay = profile->GetPrefs()->GetInteger(
      ::first_run::GetPingDelayPrefName().c_str());
  // Negative ping delay means to send ping immediately after a first search is
  // recorded.
  rlz::RLZTracker::SetRlzDelegate(
      make_scoped_ptr(new ChromeRLZTrackerDelegate));
  rlz::RLZTracker::InitRlzDelayed(
      user_manager::UserManager::Get()->IsCurrentUserNew(), ping_delay < 0,
      base::TimeDelta::FromMilliseconds(abs(ping_delay)),
      ChromeRLZTrackerDelegate::IsGoogleDefaultSearch(profile),
      ChromeRLZTrackerDelegate::IsGoogleHomepage(profile),
      ChromeRLZTrackerDelegate::IsGoogleInStartpages(profile));
#endif
}

void UserSessionManager::InitializeCerts(Profile* profile) {
  // Now that the user profile has been initialized
  // |GetNSSCertDatabaseForProfile| is safe to be used.
  if (CertLoader::IsInitialized() && base::SysInfo::IsRunningOnChromeOS()) {
    GetNSSCertDatabaseForProfile(profile,
                                 base::Bind(&OnGetNSSCertDatabaseForUser));
  }
}

void UserSessionManager::InitializeCRLSetFetcher(
    const user_manager::User* user) {
  const std::string username_hash = user->username_hash();
  if (!username_hash.empty()) {
    base::FilePath path;
    path = ProfileHelper::GetProfilePathByUserIdHash(username_hash);
    component_updater::ComponentUpdateService* cus =
        g_browser_process->component_updater();
    CRLSetFetcher* crl_set = g_browser_process->crl_set_fetcher();
    if (crl_set && cus)
      crl_set->StartInitialLoad(cus, path);
  }
}

void UserSessionManager::InitializeEVCertificatesWhitelistComponent(
    const user_manager::User* user) {
  const std::string username_hash = user->username_hash();
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  if (!username_hash.empty() && cus) {
    const base::FilePath path =
        ProfileHelper::GetProfilePathByUserIdHash(username_hash);
    RegisterEVWhitelistComponent(cus, path);
  }
}

void UserSessionManager::OnRestoreActiveSessions(
    const SessionManagerClient::ActiveSessionsMap& sessions,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Could not get list of active user sessions after crash.";
    // If we could not get list of active user sessions it is safer to just
    // sign out so that we don't get in the inconsistent state.
    DBusThreadManager::Get()->GetSessionManagerClient()->StopSession();
    return;
  }

  // One profile has been already loaded on browser start.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  DCHECK_EQ(1u, user_manager->GetLoggedInUsers().size());
  DCHECK(user_manager->GetActiveUser());
  std::string active_user_id = user_manager->GetActiveUser()->email();

  SessionManagerClient::ActiveSessionsMap::const_iterator it;
  for (it = sessions.begin(); it != sessions.end(); ++it) {
    if (active_user_id == it->first)
      continue;
    pending_user_sessions_[it->first] = it->second;
  }
  RestorePendingUserSessions();
}

void UserSessionManager::RestorePendingUserSessions() {
  if (pending_user_sessions_.empty()) {
    user_manager::UserManager::Get()->SwitchToLastActiveUser();
    NotifyPendingUserSessionsRestoreFinished();
    return;
  }

  // Get next user to restore sessions and delete it from list.
  SessionManagerClient::ActiveSessionsMap::const_iterator it =
      pending_user_sessions_.begin();
  std::string user_id = it->first;
  std::string user_id_hash = it->second;
  DCHECK(!user_id.empty());
  DCHECK(!user_id_hash.empty());
  pending_user_sessions_.erase(user_id);

  // Check that this user is not logged in yet.
  user_manager::UserList logged_in_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  bool user_already_logged_in = false;
  for (user_manager::UserList::const_iterator it = logged_in_users.begin();
       it != logged_in_users.end();
       ++it) {
    const user_manager::User* user = (*it);
    if (user->email() == user_id) {
      user_already_logged_in = true;
      break;
    }
  }
  DCHECK(!user_already_logged_in);

  if (!user_already_logged_in) {
    UserContext user_context(AccountId::FromUserEmail(user_id));
    user_context.SetUserIDHash(user_id_hash);
    user_context.SetIsUsingOAuth(false);

    // Will call OnProfilePrepared() once profile has been loaded.
    // Only handling secondary users here since primary user profile
    // (and session) has been loaded on Chrome startup.
    StartSession(user_context,
                 SECONDARY_USER_SESSION_AFTER_CRASH,
                 false,  // has_auth_cookies
                 true,   // has_active_session, this is restart after crash
                 this);
  } else {
    RestorePendingUserSessions();
  }
}

void UserSessionManager::NotifyPendingUserSessionsRestoreFinished() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  user_sessions_restored_ = true;
  user_sessions_restore_in_progress_ = false;
  FOR_EACH_OBSERVER(chromeos::UserSessionStateObserver,
                    session_state_observer_list_,
                    PendingUserSessionsRestoreFinished());
}

void UserSessionManager::UpdateEasyUnlockKeys(const UserContext& user_context) {
  // Skip key update because FakeCryptohomeClient always return success
  // and RefreshKeys op expects a failure to stop. As a result, some tests would
  // timeout.
  // TODO(xiyuan): Revisit this when adding tests.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return;

  // Only update Easy unlock keys for regular user.
  // TODO(xiyuan): Fix inconsistency user type of |user_context| introduced in
  // authenticator.
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(user_context.GetAccountId());
  if (!user || !user->HasGaiaAccount())
    return;

  // Bail if |user_context| does not have secret.
  if (user_context.GetKey()->GetSecret().empty())
    return;

  const base::ListValue* device_list = NULL;
  EasyUnlockService* easy_unlock_service = EasyUnlockService::GetForUser(*user);
  if (easy_unlock_service) {
    device_list = easy_unlock_service->GetRemoteDevices();
    easy_unlock_service->SetHardlockState(
        EasyUnlockScreenlockStateHandler::NO_HARDLOCK);
  }

  base::ListValue empty_list;
  if (!device_list)
    device_list = &empty_list;

  EasyUnlockKeyManager* key_manager = GetEasyUnlockKeyManager();
  running_easy_unlock_key_ops_ = true;
  key_manager->RefreshKeys(
      user_context, *device_list,
      base::Bind(&UserSessionManager::OnEasyUnlockKeyOpsFinished, AsWeakPtr(),
                 user_context.GetAccountId().GetUserEmail()));
}

net::URLRequestContextGetter*
UserSessionManager::GetAuthRequestContext() const {
  net::URLRequestContextGetter* auth_request_context = nullptr;

  if (StartupUtils::IsWebviewSigninEnabled()) {
    // Webview uses different partition storage than iframe. We need to get
    // cookies from the right storage for url request to get auth token into
    // session.
    content::StoragePartition* signin_partition = login::GetSigninPartition();
    if (signin_partition)
      auth_request_context = signin_partition->GetURLRequestContext();
  } else if (authenticator_.get() && authenticator_->authentication_context()) {
    auth_request_context =
        authenticator_->authentication_context()->GetRequestContext();
  }
  return auth_request_context;
}

void UserSessionManager::AttemptRestart(Profile* profile) {
  // Restart unconditionally in case if we are stuck somewhere in a session
  // restore process. http://crbug.com/520346.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(RestartOnTimeout),
      base::TimeDelta::FromSeconds(kMaxRestartDelaySeconds));

  if (CheckEasyUnlockKeyOps(base::Bind(&UserSessionManager::AttemptRestart,
                                       AsWeakPtr(), profile))) {
    return;
  }

  if (session_restore_strategy_ !=
      OAuth2LoginManager::RESTORE_FROM_COOKIE_JAR) {
    chrome::AttemptRestart();
    return;
  }

  // We can't really quit if the session restore process that mints new
  // refresh token is still in progress.
  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile);
  if (login_manager->state() != OAuth2LoginManager::SESSION_RESTORE_PREPARING &&
      login_manager->state() !=
          OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS) {
    chrome::AttemptRestart();
    return;
  }

  LOG(WARNING) << "Attempting browser restart during session restore.";
  exit_after_session_restore_ = true;
}

void UserSessionManager::OnEasyUnlockKeyOpsFinished(
    const std::string& user_id,
    bool success) {
  running_easy_unlock_key_ops_ = false;
  if (!easy_unlock_key_ops_finished_callback_.is_null())
    easy_unlock_key_ops_finished_callback_.Run();

  const user_manager::User* user = user_manager::UserManager::Get()->FindUser(
      AccountId::FromUserEmail(user_id));
  EasyUnlockService* easy_unlock_service =
        EasyUnlockService::GetForUser(*user);
  easy_unlock_service->CheckCryptohomeKeysAndMaybeHardlock();
}

void UserSessionManager::ActiveUserChanged(
    const user_manager::User* active_user) {
  if (!user_manager::UserManager::Get()->IsCurrentUserNew())
    SendUserPodsMetrics();

  Profile* profile = ProfileHelper::Get()->GetProfileByUser(active_user);
  // If profile has not yet been initialized, delay initialization of IME.
  if (!profile)
    return;

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  manager->SetState(
      GetDefaultIMEState(ProfileHelper::Get()->GetProfileByUser(active_user)));
}

scoped_refptr<input_method::InputMethodManager::State>
UserSessionManager::GetDefaultIMEState(Profile* profile) {
  scoped_refptr<input_method::InputMethodManager::State> state =
      default_ime_states_[profile];
  if (!state.get()) {
    // Profile can be NULL in tests.
    state = input_method::InputMethodManager::Get()->CreateNewState(profile);
    default_ime_states_[profile] = state;
  }
  return state;
}

EasyUnlockKeyManager* UserSessionManager::GetEasyUnlockKeyManager() {
  if (!easy_unlock_key_manager_)
    easy_unlock_key_manager_.reset(new EasyUnlockKeyManager);

  return easy_unlock_key_manager_.get();
}

void UserSessionManager::DoBrowserLaunchInternal(Profile* profile,
                                                 LoginDisplayHost* login_host,
                                                 bool locale_pref_checked) {
  if (browser_shutdown::IsTryingToQuit())
    return;

  if (!locale_pref_checked) {
    RespectLocalePreferenceWrapper(
        profile,
        base::Bind(&UserSessionManager::DoBrowserLaunchInternal, AsWeakPtr(),
                   profile, login_host, true /* locale_pref_checked */));
    return;
  }

  if (!ChromeUserManager::Get()->GetCurrentUserFlow()->ShouldLaunchBrowser()) {
    ChromeUserManager::Get()->GetCurrentUserFlow()->LaunchExtraSteps(profile);
    return;
  }

  if (RestartToApplyPerSessionFlagsIfNeed(profile, false))
    return;

  if (login_host) {
    login_host->SetStatusAreaVisible(true);
    login_host->BeforeSessionStart();
  }

  BootTimesRecorder::Get()->AddLoginTimeMarker("BrowserLaunched", false);

  VLOG(1) << "Launching browser...";
  TRACE_EVENT0("login", "LaunchBrowser");

  if (should_launch_browser_) {
    StartupBrowserCreator browser_creator;
    chrome::startup::IsFirstRun first_run =
        ::first_run::IsChromeFirstRun() ? chrome::startup::IS_FIRST_RUN
                                        : chrome::startup::IS_NOT_FIRST_RUN;

    browser_creator.LaunchBrowser(
        *base::CommandLine::ForCurrentProcess(), profile, base::FilePath(),
        chrome::startup::IS_PROCESS_STARTUP, first_run);

    // Triggers app launcher start page service to load start page web contents.
    app_list::StartPageService::Get(profile);
  } else {
    LOG(WARNING) << "Browser hasn't been launched, should_launch_browser_"
                 << " is false. This is normal in some tests.";
  }

  // Mark login host for deletion after browser starts.  This
  // guarantees that the message loop will be referenced by the
  // browser before it is dereferenced by the login host.
  if (login_host)
    login_host->Finalize();
  user_manager::UserManager::Get()->SessionStarted();
  chromeos::BootTimesRecorder::Get()->LoginDone(
      user_manager::UserManager::Get()->IsCurrentUserNew());
}

void UserSessionManager::RespectLocalePreferenceWrapper(
    Profile* profile,
    const base::Closure& callback) {
  if (browser_shutdown::IsTryingToQuit())
    return;

  const user_manager::User* const user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  locale_util::SwitchLanguageCallback locale_switched_callback(base::Bind(
      &UserSessionManager::RunCallbackOnLocaleLoaded, callback,
      base::Owned(new InputEventsBlocker)));  // Block UI events until
                                              // the ResourceBundle is
                                              // reloaded.
  if (!RespectLocalePreference(profile, user, locale_switched_callback))
    callback.Run();
}

// static
void UserSessionManager::RunCallbackOnLocaleLoaded(
    const base::Closure& callback,
    InputEventsBlocker* /* input_events_blocker */,
    const locale_util::LanguageSwitchResult& /* result */) {
  callback.Run();
}

void UserSessionManager::RemoveProfileForTesting(Profile* profile) {
  default_ime_states_.erase(profile);
}

void UserSessionManager::InjectStubUserContext(
    const UserContext& user_context) {
  injected_user_context_.reset(new UserContext(user_context));
  authenticator_ = NULL;
}

void UserSessionManager::SendUserPodsMetrics() {
  bool show_users_on_signin;
  CrosSettings::Get()->GetBoolean(kAccountsPrefShowUserNamesOnSignIn,
                                  &show_users_on_signin);
  bool is_enterprise_managed = g_browser_process->platform_part()
                                   ->browser_policy_connector_chromeos()
                                   ->IsEnterpriseManaged();
  UserPodsDisplay display;
  if (show_users_on_signin) {
    if (is_enterprise_managed)
      display = USER_PODS_DISPLAY_ENABLED_MANAGED;
    else
      display = USER_PODS_DISPLAY_ENABLED_REGULAR;
  } else {
    if (is_enterprise_managed)
      display = USER_PODS_DISPLAY_DISABLED_MANAGED;
    else
      display = USER_PODS_DISPLAY_DISABLED_REGULAR;
  }
  UMA_HISTOGRAM_ENUMERATION("UserSessionManager.UserPodsDisplay", display,
                            NUM_USER_PODS_DISPLAY);
}

void UserSessionManager::OnOAuth2TokensFetched(UserContext context) {
  if (StartupUtils::IsWebviewSigninEnabled() && TokenHandlesEnabled()) {
    CreateTokenUtilIfMissing();
    if (token_handle_util_->ShouldObtainHandle(context.GetAccountId())) {
      token_handle_fetcher_.reset(new TokenHandleFetcher(
          token_handle_util_.get(), context.GetAccountId()));
      token_handle_fetcher_->FillForNewUser(
          context.GetAccessToken(),
          base::Bind(&UserSessionManager::OnTokenHandleObtained,
                     weak_factory_.GetWeakPtr()));
    }
  }
}

void UserSessionManager::OnTokenHandleObtained(const AccountId& account_id,
                                               bool success) {
  if (!success)
    LOG(ERROR) << "OAuth2 token handle fetch failed.";
  token_handle_fetcher_.reset();
}

bool UserSessionManager::TokenHandlesEnabled() {
  if (!should_obtain_handles_)
    return false;
  bool ephemeral_users_enabled = false;
  bool show_names_on_signin = true;
  auto cros_settings = CrosSettings::Get();
  cros_settings->GetBoolean(kAccountsPrefEphemeralUsersEnabled,
                            &ephemeral_users_enabled);
  cros_settings->GetBoolean(kAccountsPrefShowUserNamesOnSignIn,
                            &show_names_on_signin);
  return show_names_on_signin && !ephemeral_users_enabled;
}

void UserSessionManager::Shutdown() {
  if (arc::ArcBridgeService::GetEnabled(
          base::CommandLine::ForCurrentProcess())) {
    DCHECK(arc::ArcServiceManager::Get());
    arc::ArcAuthService::Get()->Shutdown();
  }
  token_handle_fetcher_.reset();
  token_handle_util_.reset();
  first_run::GoodiesDisplayer::Delete();
}

void UserSessionManager::CreateTokenUtilIfMissing() {
  if (!token_handle_util_.get())
    token_handle_util_.reset(new TokenHandleUtil());
}

}  // namespace chromeos
