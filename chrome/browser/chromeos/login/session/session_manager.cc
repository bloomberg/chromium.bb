// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/session_manager.h"

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
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/login/profile_auth_data.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter_factory.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/users/user.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_brand_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/ime/input_method_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

namespace {

void InitLocaleAndInputMethodsForNewUser(PrefService* prefs) {
  // First, we'll set kLanguagePreloadEngines.
  std::string locale = g_browser_process->GetApplicationLocale();
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  std::vector<std::string> input_method_ids;
  manager->GetInputMethodUtil()->GetFirstLoginInputMethodIds(
      locale, manager->GetCurrentInputMethod(), &input_method_ids);

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

}  // namespace

// static
SessionManager* SessionManager::GetInstance() {
  return Singleton<SessionManager,
      DefaultSingletonTraits<SessionManager> >::get();
}

// static
void SessionManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kRLZBrand, std::string());
  registry->RegisterBooleanPref(prefs::kRLZDisabled, false);
}

SessionManager::SessionManager()
    : delegate_(NULL),
      has_web_auth_cookies_(false),
      exit_after_session_restore_(false),
      session_restore_strategy_(
          OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN) {
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

SessionManager::~SessionManager() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void SessionManager::OnSessionRestoreStateChanged(
    Profile* user_profile,
    OAuth2LoginManager::SessionRestoreState state) {
  User::OAuthTokenStatus user_status = User::OAUTH_TOKEN_STATUS_UNKNOWN;
  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);

  bool connection_error = false;
  switch (state) {
    case OAuth2LoginManager::SESSION_RESTORE_DONE:
      user_status = User::OAUTH2_TOKEN_STATUS_VALID;
      break;
    case OAuth2LoginManager::SESSION_RESTORE_FAILED:
      user_status = User::OAUTH2_TOKEN_STATUS_INVALID;
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

void SessionManager::OnNewRefreshTokenAvaiable(Profile* user_profile) {
  // Check if we were waiting to restart chrome.
  if (!exit_after_session_restore_)
    return;

  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);
  login_manager->RemoveObserver(this);

  // Mark user auth token status as valid.
  UserManager::Get()->SaveUserOAuthStatus(
      UserManager::Get()->GetLoggedInUser()->email(),
      User::OAUTH2_TOKEN_STATUS_VALID);

  LOG(WARNING) << "Exiting after new refresh token fetched";

  // We need to restart cleanly in this case to make sure OAuth2 RT is actually
  // saved.
  chrome::AttemptRestart();
}

void SessionManager::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  UserManager* user_manager = UserManager::Get();
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE ||
      user_manager->IsLoggedInAsGuest() || !user_manager->IsUserLoggedIn()) {
    return;
  }

  // Need to iterate over all users and their OAuth2 session state.
  const UserList& users = user_manager->GetLoggedInUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    Profile* user_profile = user_manager->GetProfileByUser(*it);
    bool should_restore_session =
        pending_restore_sessions_.find((*it)->email()) !=
            pending_restore_sessions_.end();
    OAuth2LoginManager* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);
    if (login_manager->state() ==
            OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS) {
      // If we come online for the first time after successful offline login,
      // we need to kick off OAuth token verification process again.
      login_manager->ContinueSessionRestore();
    } else if (should_restore_session) {
      pending_restore_sessions_.erase((*it)->email());
      RestoreAuthSessionImpl(user_profile, has_web_auth_cookies_);
    }
  }
}

void SessionManager::StartSession(const UserContext& user_context,
                                  scoped_refptr<Authenticator> authenticator,
                                  bool has_cookies,
                                  bool has_active_session,
                                  Delegate* delegate) {
  authenticator_ = authenticator;
  delegate_ = delegate;

  VLOG(1) << "Starting session for " << user_context.GetUserID();

  PreStartSession();
  CreateUserSession(user_context, has_cookies);

  if (!has_active_session)
    StartCrosSession();

  // TODO(nkostylev): Notify UserLoggedIn() after profile is actually
  // ready to be used (http://crbug.com/361528).
  NotifyUserLoggedIn();
  PrepareProfile();
}

void SessionManager::RestoreAuthenticationSession(Profile* user_profile) {
  UserManager* user_manager = UserManager::Get();
  // We don't need to restore session for demo/guest/stub/public account users.
  if (!user_manager->IsUserLoggedIn() ||
      user_manager->IsLoggedInAsGuest() ||
      user_manager->IsLoggedInAsPublicAccount() ||
      user_manager->IsLoggedInAsDemoUser() ||
      user_manager->IsLoggedInAsStub()) {
    return;
  }

  User* user = user_manager->GetUserByProfile(user_profile);
  DCHECK(user);
  if (!net::NetworkChangeNotifier::IsOffline()) {
    pending_restore_sessions_.erase(user->email());
    RestoreAuthSessionImpl(user_profile, false);
  } else {
    // Even if we're online we should wait till initial
    // OnConnectionTypeChanged() call. Otherwise starting fetchers too early may
    // end up canceling all request when initial network connection type is
    // processed. See http://crbug.com/121643.
    pending_restore_sessions_.insert(user->email());
  }
}

void SessionManager::InitRlz(Profile* profile) {
#if defined(ENABLE_RLZ)
  if (!g_browser_process->local_state()->HasPrefPath(prefs::kRLZBrand)) {
    // Read brand code asynchronously from an OEM data and repost ourselves.
    google_brand::chromeos::InitBrand(
        base::Bind(&SessionManager::InitRlz, AsWeakPtr(), profile));
    return;
  }
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(false),
      FROM_HERE,
      base::Bind(&base::PathExists, GetRlzDisabledFlagPath()),
      base::Bind(&SessionManager::InitRlzImpl, AsWeakPtr(), profile));
#endif
}

OAuth2LoginManager::SessionRestoreStrategy
SessionManager::GetSigninSessionRestoreStrategy() {
  return session_restore_strategy_;
}

// static
void SessionManager::SetFirstLoginPrefs(PrefService* prefs) {
  VLOG(1) << "Setting first login prefs";
  InitLocaleAndInputMethodsForNewUser(prefs);
}

void SessionManager::CreateUserSession(const UserContext& user_context,
                                       bool has_cookies) {
  user_context_ = user_context;
  has_web_auth_cookies_ = has_cookies;
  InitSessionRestoreStrategy();
}

void SessionManager::PreStartSession() {
  // Switch log file as soon as possible.
  if (base::SysInfo::IsRunningOnChromeOS())
    logging::RedirectChromeLogging(*(CommandLine::ForCurrentProcess()));
}

void SessionManager::StartCrosSession() {
  BootTimesLoader* btl = BootTimesLoader::Get();
  btl->AddLoginTimeMarker("StartSession-Start", false);
  DBusThreadManager::Get()->GetSessionManagerClient()->
      StartSession(user_context_.GetUserID());
  btl->AddLoginTimeMarker("StartSession-End", false);
}

void SessionManager:: NotifyUserLoggedIn() {
  BootTimesLoader* btl = BootTimesLoader::Get();
  btl->AddLoginTimeMarker("UserLoggedIn-Start", false);
  UserManager* user_manager = UserManager::Get();
  user_manager->UserLoggedIn(user_context_.GetUserID(),
                             user_context_.GetUserIDHash(),
                             false);
  btl->AddLoginTimeMarker("UserLoggedIn-End", false);
}

void SessionManager::PrepareProfile() {
  UserManager* user_manager = UserManager::Get();
  bool is_demo_session =
      DemoAppLauncher::IsDemoAppSession(user_context_.GetUserID());

  // TODO(nkostylev): Figure out whether demo session is using the right profile
  // path or not. See https://codereview.chromium.org/171423009
  g_browser_process->profile_manager()->CreateProfileAsync(
      user_manager->GetUserProfileDir(user_context_.GetUserID()),
      base::Bind(&SessionManager::OnProfileCreated, AsWeakPtr(),
                 user_context_.GetUserID(), is_demo_session),
      base::string16(), base::string16(), std::string());
}

void SessionManager::OnProfileCreated(const std::string& user_id,
                                      bool is_incognito_profile,
                                      Profile* profile,
                                      Profile::CreateStatus status) {
  CHECK(profile);

  switch (status) {
    case Profile::CREATE_STATUS_CREATED:
      // Profile created but before initializing extensions and promo resources.
      InitProfilePreferences(profile, user_id);
      break;
    case Profile::CREATE_STATUS_INITIALIZED:
      // Profile is created, extensions and promo resources are initialized.
      // At this point all other Chrome OS services will be notified that it is
      // safe to use this profile.
      UserProfileInitialized(profile, is_incognito_profile);
      break;
    case Profile::CREATE_STATUS_LOCAL_FAIL:
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::CREATE_STATUS_CANCELED:
    case Profile::MAX_CREATE_STATUS:
      NOTREACHED();
      break;
  }
}

void SessionManager::InitProfilePreferences(Profile* profile,
                                            const std::string& user_id) {
  if (UserManager::Get()->IsCurrentUserNew())
    SetFirstLoginPrefs(profile->GetPrefs());

  if (UserManager::Get()->IsLoggedInAsLocallyManagedUser()) {
    User* active_user = UserManager::Get()->GetActiveUser();
    std::string managed_user_sync_id =
        UserManager::Get()->GetSupervisedUserManager()->
            GetUserSyncId(active_user->email());
    profile->GetPrefs()->SetString(prefs::kManagedUserId, managed_user_sync_id);
  } else if (UserManager::Get()->IsLoggedInAsRegularUser()) {
    // Make sure that the google service username is properly set (we do this
    // on every sign in, not just the first login, to deal with existing
    // profiles that might not have it set yet).
    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfile(profile);
    signin_manager->SetAuthenticatedUsername(user_id);
  }
}

void SessionManager::UserProfileInitialized(Profile* profile,
                                            bool is_incognito_profile) {
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
    // Transfer proxy authentication cache, cookies (optionally) and server
    // bound certs from the profile that was used for authentication.  This
    // profile contains cookies that auth extension should have already put in
    // place that will ensure that the newly created session is authenticated
    // for the websites that work with the used authentication schema.
    ProfileAuthData::Transfer(
        authenticator_->authentication_profile(),
        profile,
        has_web_auth_cookies_,  // transfer_cookies
        base::Bind(&SessionManager::CompleteProfileCreateAfterAuthTransfer,
                   AsWeakPtr(),
                   profile));
    return;
  }

  FinalizePrepareProfile(profile);
}

void SessionManager::CompleteProfileCreateAfterAuthTransfer(Profile* profile) {
  RestoreAuthSessionImpl(profile, has_web_auth_cookies_);
  FinalizePrepareProfile(profile);
}

void SessionManager::FinalizePrepareProfile(Profile* profile) {
  BootTimesLoader* btl = BootTimesLoader::Get();

  // Own TPM device if, for any reason, it has not been done in EULA screen.
  CryptohomeClient* client = DBusThreadManager::Get()->GetCryptohomeClient();
  btl->AddLoginTimeMarker("TPMOwn-Start", false);
  if (cryptohome_util::TpmIsEnabled() && !cryptohome_util::TpmIsBeingOwned()) {
    if (cryptohome_util::TpmIsOwned()) {
      client->CallTpmClearStoredPasswordAndBlock();
    } else {
      client->TpmCanAttemptOwnership(EmptyVoidDBusMethodCallback());
    }
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

  // Send the notification before creating the browser so additional objects
  // that need the profile (e.g. the launcher) can be created first.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(profile));

  // Initialize RLZ only for primary user.
  if (user_manager->GetPrimaryUser() ==
      user_manager->GetUserByProfile(profile)) {
    InitRlz(profile);
  }

  // TODO(altimofeev): This pointer should probably never be NULL, but it looks
  // like LoginUtilsImpl::OnProfileCreated() may be getting called before
  // SessionManager::PrepareProfile() has set |delegate_| when Chrome is killed
  // during shutdown in tests -- see http://crosbug.com/18269.  Replace this
  // 'if' statement with a CHECK(delegate_) once the underlying issue is
  // resolved.
  if (delegate_)
    delegate_->OnProfilePrepared(profile);
}

void SessionManager::InitSessionRestoreStrategy() {
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

    DCHECK(!has_web_auth_cookies_);
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

  if (has_web_auth_cookies_) {
    session_restore_strategy_ = OAuth2LoginManager::RESTORE_FROM_COOKIE_JAR;
  } else if (!user_context_.GetAuthCode().empty()) {
    session_restore_strategy_ = OAuth2LoginManager::RESTORE_FROM_AUTH_CODE;
  } else {
    session_restore_strategy_ =
        OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN;
  }
}

void SessionManager::RestoreAuthSessionImpl(Profile* profile,
                                            bool restore_from_auth_cookies) {
  CHECK((authenticator_.get() && authenticator_->authentication_profile()) ||
        !restore_from_auth_cookies);

  if (chrome::IsRunningInForcedAppMode() ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kOobeSkipPostLogin)) {
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

void SessionManager::InitRlzImpl(Profile* profile, bool disabled) {
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

}  // namespace chromeos
