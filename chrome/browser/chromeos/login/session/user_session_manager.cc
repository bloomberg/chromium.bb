// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/user_session_manager.h"

#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/login/profile_auth_data.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter_factory.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_brand_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/crl_set_fetcher.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cert_loader.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

namespace {

void InitLocaleAndInputMethodsForNewUser(
    PrefService* prefs,
    const std::string& public_session_locale,
    const std::string& public_session_input_method) {
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
  std::vector<std::string> input_method_ids;

  if (!public_session_input_method.empty()) {
    // If this is a public session and the user chose a
    // |public_session_input_method|, set kLanguagePreloadEngines to this input
    // method only.
    input_method_ids.push_back(public_session_input_method);
  } else {
    // Otherwise, set kLanguagePreloadEngines to a list of input methods derived
    // from the |locale| and the currently active input method.
    manager->GetInputMethodUtil()->GetFirstLoginInputMethodIds(
        locale, manager->GetCurrentInputMethod(), &input_method_ids);
  }

  // Save the input methods in the user's preferences.
  StringPrefMember language_preload_engines;
  language_preload_engines.Init(prefs::kLanguagePreloadEngines, prefs);
  language_preload_engines.SetValue(JoinString(input_method_ids, ','));
  BootTimesLoader::Get()->AddLoginTimeMarker("IMEStarted", false);

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
  StringPrefMember language_preferred_languages;
  language_preferred_languages.Init(prefs::kLanguagePreferredLanguages, prefs);
  language_preferred_languages.SetValue(JoinString(language_codes, ','));
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

}  // namespace

#if defined(ENABLE_RLZ)
void UserSessionManagerDelegate::OnRlzInitialized() {
}
#endif

UserSessionManagerDelegate::~UserSessionManagerDelegate() {
}

void UserSessionStateObserver::PendingUserSessionsRestoreFinished() {
}

UserSessionStateObserver::~UserSessionStateObserver() {
}

// static
UserSessionManager* UserSessionManager::GetInstance() {
  return Singleton<UserSessionManager,
      DefaultSingletonTraits<UserSessionManager> >::get();
}

// static
void UserSessionManager::OverrideHomedir() {
  // Override user homedir, check for ProfileManager being initialized as
  // it may not exist in unit tests.
  if (g_browser_process->profile_manager()) {
    UserManager* user_manager = UserManager::Get();
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
}

UserSessionManager::UserSessionManager()
    : delegate_(NULL),
      has_auth_cookies_(false),
      user_sessions_restored_(false),
      exit_after_session_restore_(false),
      session_restore_strategy_(
          OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN) {
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

UserSessionManager::~UserSessionManager() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void UserSessionManager::StartSession(
    const UserContext& user_context,
    scoped_refptr<Authenticator> authenticator,
    bool has_auth_cookies,
    bool has_active_session,
    UserSessionManagerDelegate* delegate) {
  authenticator_ = authenticator;
  delegate_ = delegate;

  VLOG(1) << "Starting session for " << user_context.GetUserID();

  PreStartSession();
  CreateUserSession(user_context, has_auth_cookies);

  if (!has_active_session)
    StartCrosSession();

  // TODO(nkostylev): Notify UserLoggedIn() after profile is actually
  // ready to be used (http://crbug.com/361528).
  NotifyUserLoggedIn();
  PrepareProfile();
}

void UserSessionManager::PerformPostUserLoggedInActions() {
  UserManager* user_manager = UserManager::Get();
  if (user_manager->GetLoggedInUsers().size() == 1) {
    // Owner must be first user in session. DeviceSettingsService can't deal
    // with multiple user and will mix up ownership, crbug.com/230018.
    OwnerSettingsServiceFactory::GetInstance()->
        SetUsername(user_manager->GetActiveUser()->email());

    if (NetworkPortalDetector::IsInitialized()) {
      NetworkPortalDetector::Get()->SetStrategy(
          PortalDetectorStrategy::STRATEGY_ID_SESSION);
    }
  }
}

void UserSessionManager::RestoreAuthenticationSession(Profile* user_profile) {
  UserManager* user_manager = UserManager::Get();
  // We need to restore session only for logged in regular (GAIA) users.
  // Note: stub user is a special case that is used for tests, running
  // linux_chromeos build on dev workstations w/o user_id parameters.
  // Stub user is considered to be a regular GAIA user but it has special
  // user_id (kStubUser) and certain services like restoring OAuth session are
  // explicitly disabled for it.
  if (!user_manager->IsUserLoggedIn() ||
      !user_manager->IsLoggedInAsRegularUser() ||
      user_manager->IsLoggedInAsStub()) {
    return;
  }

  user_manager::User* user =
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
  DBusThreadManager::Get()->GetSessionManagerClient()->RetrieveActiveSessions(
      base::Bind(&UserSessionManager::OnRestoreActiveSessions,
                 base::Unretained(this)));
}

bool UserSessionManager::UserSessionsRestored() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return user_sessions_restored_;
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
      base::WorkerPool::GetTaskRunner(false),
      FROM_HERE,
      base::Bind(&base::PathExists, GetRlzDisabledFlagPath()),
      base::Bind(&UserSessionManager::InitRlzImpl, AsWeakPtr(), profile));
#endif
}

OAuth2LoginManager::SessionRestoreStrategy
UserSessionManager::GetSigninSessionRestoreStrategy() {
  return session_restore_strategy_;
}

// static
void UserSessionManager::SetFirstLoginPrefs(
    PrefService* prefs,
    const std::string& public_session_locale,
    const std::string& public_session_input_method) {
  VLOG(1) << "Setting first login prefs";
  InitLocaleAndInputMethodsForNewUser(prefs,
                                      public_session_locale,
                                      public_session_input_method);
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

bool UserSessionManager::RespectLocalePreference(
    Profile* profile,
    const user_manager::User* user,
    scoped_ptr<locale_util::SwitchLanguageCallback> callback) const {
  // TODO(alemate): http://crbug.com/288941 : Respect preferred language list in
  // the Google user profile.
  if (g_browser_process == NULL)
    return false;

  UserManager* user_manager = UserManager::Get();
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
  const bool enable_layouts = UserManager::Get()->IsLoggedInAsGuest();
  locale_util::SwitchLanguage(pref_locale,
                              enable_layouts,
                              false /* login_layouts_only */,
                              callback.Pass());

  return true;
}

void UserSessionManager::AddSessionStateObserver(
    UserSessionStateObserver* observer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  session_state_observer_list_.AddObserver(observer);
}

void UserSessionManager::RemoveSessionStateObserver(
    UserSessionStateObserver* observer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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
    UserManager::Get()->SaveUserOAuthStatus(
        UserManager::Get()->GetLoggedInUser()->email(),
        user_status);
  }

  login_manager->RemoveObserver(this);
}

void UserSessionManager::OnNewRefreshTokenAvaiable(Profile* user_profile) {
  // Check if we were waiting to restart chrome.
  if (!exit_after_session_restore_)
    return;

  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);
  login_manager->RemoveObserver(this);

  // Mark user auth token status as valid.
  UserManager::Get()->SaveUserOAuthStatus(
      UserManager::Get()->GetLoggedInUser()->email(),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

  VLOG(1) << "Exiting after new refresh token fetched";

  // We need to restart cleanly in this case to make sure OAuth2 RT is actually
  // saved.
  chrome::AttemptRestart();
}

void UserSessionManager::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  bool is_running_test =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestName) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(::switches::kTestType);
  UserManager* user_manager = UserManager::Get();
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE ||
      !user_manager->IsUserLoggedIn() ||
      !user_manager->IsLoggedInAsRegularUser() ||
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

    Profile* user_profile = ProfileHelper::Get()->GetProfileByUser(*it);
    bool should_restore_session =
        pending_signin_restore_sessions_.find((*it)->email()) !=
        pending_signin_restore_sessions_.end();
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);
    if (login_manager->state() ==
            OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS) {
      // If we come online for the first time after successful offline login,
      // we need to kick off OAuth token verification process again.
      login_manager->ContinueSessionRestore();
    } else if (should_restore_session) {
      pending_signin_restore_sessions_.erase((*it)->email());
      RestoreAuthSessionImpl(user_profile, false /* has_auth_cookies */);
    }
  }
}

void UserSessionManager::OnProfilePrepared(Profile* profile) {
  LoginUtils::Get()->DoBrowserLaunch(profile, NULL);  // host_, not needed here

  if (!CommandLine::ForCurrentProcess()->HasSwitch(::switches::kTestName)) {
    // Did not log in (we crashed or are debugging), need to restore Sync.
    // TODO(nkostylev): Make sure that OAuth state is restored correctly for all
    // users once it is fully multi-profile aware. http://crbug.com/238987
    // For now if we have other user pending sessions they'll override OAuth
    // session restore for previous users.
    UserSessionManager::GetInstance()->RestoreAuthenticationSession(profile);
  }

  // Restore other user sessions if any.
  RestorePendingUserSessions();
}

void UserSessionManager::CreateUserSession(const UserContext& user_context,
                                           bool has_auth_cookies) {
  user_context_ = user_context;
  has_auth_cookies_ = has_auth_cookies;
  InitSessionRestoreStrategy();
}

void UserSessionManager::PreStartSession() {
  // Switch log file as soon as possible.
  if (base::SysInfo::IsRunningOnChromeOS())
    logging::RedirectChromeLogging(*(CommandLine::ForCurrentProcess()));
}

void UserSessionManager::StartCrosSession() {
  BootTimesLoader* btl = BootTimesLoader::Get();
  btl->AddLoginTimeMarker("StartSession-Start", false);
  DBusThreadManager::Get()->GetSessionManagerClient()->
      StartSession(user_context_.GetUserID());
  btl->AddLoginTimeMarker("StartSession-End", false);
}

void UserSessionManager::NotifyUserLoggedIn() {
  BootTimesLoader* btl = BootTimesLoader::Get();
  btl->AddLoginTimeMarker("UserLoggedIn-Start", false);
  UserManager* user_manager = UserManager::Get();
  user_manager->UserLoggedIn(user_context_.GetUserID(),
                             user_context_.GetUserIDHash(),
                             false);
  btl->AddLoginTimeMarker("UserLoggedIn-End", false);
}

void UserSessionManager::PrepareProfile() {
  bool is_demo_session =
      DemoAppLauncher::IsDemoAppSession(user_context_.GetUserID());

  // TODO(nkostylev): Figure out whether demo session is using the right profile
  // path or not. See https://codereview.chromium.org/171423009
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileHelper::GetUserProfileDirByUserId(user_context_.GetUserID()),
      base::Bind(&UserSessionManager::OnProfileCreated,
                 AsWeakPtr(),
                 user_context_,
                 is_demo_session),
      base::string16(),
      base::string16(),
      std::string());
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
      UserProfileInitialized(profile,
                             is_incognito_profile,
                             user_context.GetUserID());
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
  if (UserManager::Get()->IsCurrentUserNew()) {
    SetFirstLoginPrefs(profile->GetPrefs(),
                       user_context.GetPublicSessionLocale(),
                       user_context.GetPublicSessionInputMethod());
  }

  if (UserManager::Get()->IsLoggedInAsSupervisedUser()) {
    user_manager::User* active_user = UserManager::Get()->GetActiveUser();
    std::string supervised_user_sync_id =
        UserManager::Get()->GetSupervisedUserManager()->
            GetUserSyncId(active_user->email());
    profile->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                   supervised_user_sync_id);
  } else if (UserManager::Get()->IsLoggedInAsRegularUser()) {
    // Make sure that the google service username is properly set (we do this
    // on every sign in, not just the first login, to deal with existing
    // profiles that might not have it set yet).
    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfile(profile);
    signin_manager->SetAuthenticatedUsername(user_context.GetUserID());
  }
}

void UserSessionManager::UserProfileInitialized(Profile* profile,
                                                bool is_incognito_profile,
                                                const std::string& user_id) {
  if (is_incognito_profile) {
    profile->OnLogin();
    // Send the notification before creating the browser so additional objects
    // that need the profile (e.g. the launcher) can be created first.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
        content::NotificationService::AllSources(),
        content::Details<Profile>(profile));

    if (delegate_)
      delegate_->OnProfilePrepared(profile);

    return;
  }

  BootTimesLoader* btl = BootTimesLoader::Get();
  btl->AddLoginTimeMarker("UserProfileGotten", false);

  if (user_context_.IsUsingOAuth()) {
    // Retrieve the policy that indicates whether to continue copying
    // authentication cookies set by a SAML IdP on subsequent logins after the
    // first.
    bool transfer_saml_auth_cookies_on_subsequent_login = false;
    if (has_auth_cookies_ &&
        g_browser_process->platform_part()->
            browser_policy_connector_chromeos()->GetUserAffiliation(user_id) ==
                policy::USER_AFFILIATION_MANAGED) {
      CrosSettings::Get()->GetBoolean(
          kAccountsPrefTransferSAMLCookies,
          &transfer_saml_auth_cookies_on_subsequent_login);
    }

    // Transfers authentication-related data from the profile that was used for
    // authentication to the user's profile. The proxy authentication state is
    // transferred unconditionally. If the user authenticated via an auth
    // extension, authentication cookies and channel IDs will be transferred as
    // well when the user's cookie jar is empty. If the cookie jar is not empty,
    // the authentication states in the login profile and the user's profile
    // must be merged using /MergeSession instead. Authentication cookies set by
    // a SAML IdP will also be transferred when the user's cookie jar is not
    // empty if |transfer_saml_auth_cookies_on_subsequent_login| is true.
    const bool transfer_auth_cookies_and_channel_ids_on_first_login =
        has_auth_cookies_;
    ProfileAuthData::Transfer(
        authenticator_->authentication_profile(),
        profile,
        transfer_auth_cookies_and_channel_ids_on_first_login,
        transfer_saml_auth_cookies_on_subsequent_login,
        base::Bind(&UserSessionManager::CompleteProfileCreateAfterAuthTransfer,
                   AsWeakPtr(),
                   profile));
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
  BootTimesLoader* btl = BootTimesLoader::Get();

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

  UserManager* user_manager = UserManager::Get();
  if (user_manager->IsLoggedInAsRegularUser()) {
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
  }

  // TODO(nkostylev): This pointer should probably never be NULL, but it looks
  // like LoginUtilsImpl::OnProfileCreated() may be getting called before
  // UserSessionManager::PrepareProfile() has set |delegate_| when Chrome is
  // killed during shutdown in tests -- see http://crosbug.com/18269.  Replace
  // this 'if' statement with a CHECK(delegate_) once the underlying issue is
  // resolved.
  if (delegate_)
    delegate_->OnProfilePrepared(profile);
}

void UserSessionManager::InitSessionRestoreStrategy() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool in_app_mode = chrome::IsRunningInForcedAppMode();

  // Are we in kiosk app mode?
  if (in_app_mode) {
    if (command_line->HasSwitch(::switches::kAppModeOAuth2Token)) {
      oauth2_refresh_token_ = command_line->GetSwitchValueASCII(
          ::switches::kAppModeOAuth2Token);
    }

    if (command_line->HasSwitch(::switches::kAppModeAuthCode)) {
      user_context_.SetAuthCode(command_line->GetSwitchValueASCII(
          ::switches::kAppModeAuthCode));
    }

    DCHECK(!has_auth_cookies_);
    if (!user_context_.GetAuthCode().empty()) {
      session_restore_strategy_ = OAuth2LoginManager::RESTORE_FROM_AUTH_CODE;
    } else if (!oauth2_refresh_token_.empty()) {
      session_restore_strategy_ =
          OAuth2LoginManager::RESTORE_FROM_PASSED_OAUTH2_REFRESH_TOKEN;
    } else {
      session_restore_strategy_ =
          OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN;
    }
    return;
  }

  if (has_auth_cookies_) {
    session_restore_strategy_ = OAuth2LoginManager::RESTORE_FROM_COOKIE_JAR;
  } else if (!user_context_.GetAuthCode().empty()) {
    session_restore_strategy_ = OAuth2LoginManager::RESTORE_FROM_AUTH_CODE;
  } else {
    session_restore_strategy_ =
        OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN;
  }
}

void UserSessionManager::RestoreAuthSessionImpl(Profile* profile,
                                            bool restore_from_auth_cookies) {
  CHECK((authenticator_.get() && authenticator_->authentication_profile()) ||
        !restore_from_auth_cookies);

  if (chrome::IsRunningInForcedAppMode() ||
      CommandLine::ForCurrentProcess()->HasSwitch(
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
  login_manager->RestoreSession(
      authenticator_.get() && authenticator_->authentication_profile()
          ? authenticator_->authentication_profile()->GetRequestContext()
          : NULL,
      session_restore_strategy_,
      oauth2_refresh_token_,
      user_context_.GetAuthCode());
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
    RLZTracker::ClearRlzState();
    local_state->SetBoolean(prefs::kRLZDisabled, disabled);
  }
  // Init the RLZ library.
  int ping_delay = profile->GetPrefs()->GetInteger(
      first_run::GetPingDelayPrefName().c_str());
  // Negative ping delay means to send ping immediately after a first search is
  // recorded.
  RLZTracker::InitRlzFromProfileDelayed(
      profile, UserManager::Get()->IsCurrentUserNew(),
      ping_delay < 0, base::TimeDelta::FromMilliseconds(abs(ping_delay)));
  if (delegate_)
    delegate_->OnRlzInitialized();
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
  UserManager* user_manager = UserManager::Get();
  DCHECK(user_manager->GetLoggedInUsers().size() == 1);
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
      UserManager::Get()->GetLoggedInUsers();
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
    UserContext user_context(user_id);
    user_context.SetUserIDHash(user_id_hash);
    user_context.SetIsUsingOAuth(false);

    // Will call OnProfilePrepared() once profile has been loaded.
    StartSession(user_context,
                 NULL,   // authenticator
                 false,  // has_auth_cookies
                 true,   // has_active_session
                 this);
  } else {
    RestorePendingUserSessions();
  }
}

void UserSessionManager::NotifyPendingUserSessionsRestoreFinished() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  user_sessions_restored_ = true;
  FOR_EACH_OBSERVER(UserSessionStateObserver,
                    session_state_observer_list_,
                    PendingUserSessionsRestoreFinished());
}

}  // namespace chromeos
