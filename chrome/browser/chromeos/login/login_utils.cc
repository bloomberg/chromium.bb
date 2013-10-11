// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_utils.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/chrome_restart_request.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/parallel_authenticator.h"
#include "chrome/browser/chromeos/login/profile_auth_data.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_util_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/pref_service_flags_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chromeos {

namespace {

#if defined(ENABLE_RLZ)
// Flag file that disables RLZ tracking, when present.
const base::FilePath::CharType kRLZDisabledFlagName[] =
    FILE_PATH_LITERAL(".rlz_disabled");

base::FilePath GetRlzDisabledFlagPath() {
  return file_util::GetHomeDir().Append(kRLZDisabledFlagName);
}
#endif

}  // namespace

class LoginUtilsImpl
    : public LoginUtils,
      public OAuth2LoginManager::Observer,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public base::SupportsWeakPtr<LoginUtilsImpl> {
 public:
  LoginUtilsImpl()
      : using_oauth_(false),
        has_web_auth_cookies_(false),
        delegate_(NULL),
        should_restore_auth_session_(false),
        exit_after_session_restore_(false),
        session_restore_strategy_(
            OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN) {
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  }

  virtual ~LoginUtilsImpl() {
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  }

  // LoginUtils implementation:
  virtual void DoBrowserLaunch(Profile* profile,
                               LoginDisplayHost* login_host) OVERRIDE;
  virtual void PrepareProfile(
      const UserContext& user_context,
      const std::string& display_email,
      bool using_oauth,
      bool has_cookies,
      bool has_active_session,
      LoginUtils::Delegate* delegate) OVERRIDE;
  virtual void DelegateDeleted(LoginUtils::Delegate* delegate) OVERRIDE;
  virtual void CompleteOffTheRecordLogin(const GURL& start_url) OVERRIDE;
  virtual void SetFirstLoginPrefs(PrefService* prefs) OVERRIDE;
  virtual scoped_refptr<Authenticator> CreateAuthenticator(
      LoginStatusConsumer* consumer) OVERRIDE;
  virtual void RestoreAuthenticationSession(Profile* profile) OVERRIDE;
  virtual void InitRlzDelayed(Profile* user_profile) OVERRIDE;

  // OAuth2LoginManager::Observer overrides.
  virtual void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) OVERRIDE;
  virtual void OnNewRefreshTokenAvaiable(Profile* user_profile) OVERRIDE;
  virtual void OnSessionAuthenticated(Profile* user_profile) OVERRIDE;

  // net::NetworkChangeNotifier::ConnectionTypeObserver overrides.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

 private:
  // Restarts OAuth session authentication check.
  void KickStartAuthentication(Profile* profile);

  // Callback for Profile::CREATE_STATUS_CREATED profile state.
  // Initializes basic preferences for newly created profile. Any other
  // early profile initialization that needs to happen before
  // ProfileManager::DoFinalInit() gets called is done here.
  void InitProfilePreferences(Profile* user_profile,
                              const std::string& email);

  // Callback for asynchronous profile creation.
  void OnProfileCreated(const std::string& email,
                        Profile* profile,
                        Profile::CreateStatus status);

  // Callback for Profile::CREATE_STATUS_INITIALIZED profile state.
  // Profile is created, extensions and promo resources are initialized.
  void UserProfileInitialized(Profile* user_profile);

  // Callback to resume profile creation after transferring auth data from
  // the authentication profile.
  void CompleteProfileCreate(Profile* user_profile);

  // Finalized profile preparation.
  void FinalizePrepareProfile(Profile* user_profile);

  // Initializes member variables needed for session restore process via
  // OAuthLoginManager.
  void InitSessionRestoreStrategy();

  // Restores GAIA auth cookies for the created user profile from OAuth2 token.
  void RestoreAuthSession(Profile* user_profile,
                          bool restore_from_auth_cookies);

  // Initializes RLZ. If |disabled| is true, RLZ pings are disabled.
  void InitRlz(Profile* user_profile, bool disabled);

  // Starts signing related services. Initiates TokenService token retrieval.
  void StartSignedInServices(Profile* profile);

  // Attempts exiting browser process and esures this does not happen
  // while we are still fetching new OAuth refresh tokens.
  void AttemptExit(Profile* profile);

  UserContext user_context_;
  bool using_oauth_;

  // True if the authentication profile's cookie jar should contain
  // authentication cookies from the authentication extension log in flow.
  bool has_web_auth_cookies_;
  // Has to be scoped_refptr, see comment for CreateAuthenticator(...).
  scoped_refptr<Authenticator> authenticator_;

  // Delegate to be fired when the profile will be prepared.
  LoginUtils::Delegate* delegate_;

  // True if should restore authentication session when notified about
  // online state change.
  bool should_restore_auth_session_;

  // True if we should restart chrome right after session restore.
  bool exit_after_session_restore_;

  // Sesion restore strategy.
  OAuth2LoginManager::SessionRestoreStrategy session_restore_strategy_;
  // OAuth2 refresh token for session restore.
  std::string oauth2_refresh_token_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsImpl);
};

class LoginUtilsWrapper {
 public:
  static LoginUtilsWrapper* GetInstance() {
    return Singleton<LoginUtilsWrapper>::get();
  }

  LoginUtils* get() {
    base::AutoLock create(create_lock_);
    if (!ptr_.get())
      reset(new LoginUtilsImpl);
    return ptr_.get();
  }

  void reset(LoginUtils* ptr) {
    ptr_.reset(ptr);
  }

 private:
  friend struct DefaultSingletonTraits<LoginUtilsWrapper>;

  LoginUtilsWrapper() {}

  base::Lock create_lock_;
  scoped_ptr<LoginUtils> ptr_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsWrapper);
};

void LoginUtilsImpl::DoBrowserLaunch(Profile* profile,
                                     LoginDisplayHost* login_host) {
  if (browser_shutdown::IsTryingToQuit())
    return;

  User* user = UserManager::Get()->GetUserByProfile(profile);
  if (user != NULL)
    UserManager::Get()->RespectLocalePreference(profile, user);

  if (!UserManager::Get()->GetCurrentUserFlow()->ShouldLaunchBrowser()) {
    UserManager::Get()->GetCurrentUserFlow()->LaunchExtraSteps(profile);
    return;
  }

  CommandLine user_flags(CommandLine::NO_PROGRAM);
  about_flags::PrefServiceFlagsStorage flags_storage_(profile->GetPrefs());
  about_flags::ConvertFlagsToSwitches(&flags_storage_, &user_flags,
                                      about_flags::kAddSentinels);
  // Only restart if needed and if not going into managed mode.
  // Don't restart browser if it is not first profile in session.
  if (UserManager::Get()->GetLoggedInUsers().size() == 1 &&
      !UserManager::Get()->IsLoggedInAsLocallyManagedUser() &&
      !about_flags::AreSwitchesIdenticalToCurrentCommandLine(
          user_flags, *CommandLine::ForCurrentProcess())) {
    CommandLine::StringVector flags;
    // argv[0] is the program name |CommandLine::NO_PROGRAM|.
    flags.assign(user_flags.argv().begin() + 1, user_flags.argv().end());
    VLOG(1) << "Restarting to apply per-session flags...";
    DBusThreadManager::Get()->GetSessionManagerClient()->SetFlagsForUser(
        UserManager::Get()->GetActiveUser()->email(), flags);
    AttemptExit(profile);
    return;
  }

  if (login_host) {
    login_host->SetStatusAreaVisible(true);
    login_host->BeforeSessionStart();
  }

  BootTimesLoader::Get()->AddLoginTimeMarker("BrowserLaunched", false);

  VLOG(1) << "Launching browser...";
  StartupBrowserCreator browser_creator;
  int return_code;
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;

  browser_creator.LaunchBrowser(*CommandLine::ForCurrentProcess(),
                                profile,
                                base::FilePath(),
                                chrome::startup::IS_PROCESS_STARTUP,
                                first_run,
                                &return_code);

  // Triggers app launcher start page service to load start page web contents.
  app_list::StartPageService::Get(profile);

  // Mark login host for deletion after browser starts.  This
  // guarantees that the message loop will be referenced by the
  // browser before it is dereferenced by the login host.
  if (login_host)
    login_host->Finalize();
  UserManager::Get()->SessionStarted();
}

void LoginUtilsImpl::PrepareProfile(
    const UserContext& user_context,
    const std::string& display_email,
    bool using_oauth,
    bool has_cookies,
    bool has_active_session,
    LoginUtils::Delegate* delegate) {
  BootTimesLoader* btl = BootTimesLoader::Get();

  VLOG(1) << "Completing login for " << user_context.username;

  if (!has_active_session) {
    btl->AddLoginTimeMarker("StartSession-Start", false);
    DBusThreadManager::Get()->GetSessionManagerClient()->StartSession(
        user_context.username);
    btl->AddLoginTimeMarker("StartSession-End", false);
  }

  btl->AddLoginTimeMarker("UserLoggedIn-Start", false);
  UserManager* user_manager = UserManager::Get();
  user_manager->UserLoggedIn(user_context.username,
                             user_context.username_hash,
                             false);
  btl->AddLoginTimeMarker("UserLoggedIn-End", false);

  // Switch log file as soon as possible.
  if (base::SysInfo::IsRunningOnChromeOS())
    logging::RedirectChromeLogging(*(CommandLine::ForCurrentProcess()));

  // Update user's displayed email.
  if (!display_email.empty())
    user_manager->SaveUserDisplayEmail(user_context.username, display_email);

  user_context_ = user_context;

  using_oauth_ = using_oauth;
  has_web_auth_cookies_ = has_cookies;
  delegate_ = delegate;
  InitSessionRestoreStrategy();

  // Can't use display_email because it is empty when existing user logs in
  // using sing-in pod on login screen (i.e. user didn't type email).
  g_browser_process->profile_manager()->CreateProfileAsync(
      user_manager->GetUserProfileDir(user_context.username),
      base::Bind(&LoginUtilsImpl::OnProfileCreated, AsWeakPtr(),
                 user_context.username),
      string16(), string16(), std::string());
}

void LoginUtilsImpl::DelegateDeleted(LoginUtils::Delegate* delegate) {
  if (delegate_ == delegate)
    delegate_ = NULL;
}

void LoginUtilsImpl::InitProfilePreferences(Profile* user_profile,
    const std::string& email) {
  if (UserManager::Get()->IsCurrentUserNew())
    SetFirstLoginPrefs(user_profile->GetPrefs());

  if (UserManager::Get()->IsLoggedInAsLocallyManagedUser()) {
    User* active_user = UserManager::Get()->GetActiveUser();
    std::string managed_user_sync_id =
        UserManager::Get()->GetManagedUserSyncId(active_user->email());

    // TODO(ibraaaa): Remove that when 97% of our users are using M31.
    // http://crbug.com/276163
    if (managed_user_sync_id.empty())
      managed_user_sync_id = "DUMMY_ID";

    user_profile->GetPrefs()->SetString(prefs::kManagedUserId,
                                        managed_user_sync_id);
  } else {
    // Make sure that the google service username is properly set (we do this
    // on every sign in, not just the first login, to deal with existing
    // profiles that might not have it set yet).
    StringPrefMember google_services_username;
    google_services_username.Init(prefs::kGoogleServicesUsername,
                                  user_profile->GetPrefs());
    const User* user = UserManager::Get()->FindUser(email);
    google_services_username.SetValue(user ? user->display_email() : email);
  }
}

void LoginUtilsImpl::InitSessionRestoreStrategy() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool in_app_mode = chrome::IsRunningInForcedAppMode();

  // Are we in kiosk app mode?
  if (in_app_mode) {
    if (command_line->HasSwitch(::switches::kAppModeOAuth2Token)) {
      oauth2_refresh_token_ = command_line->GetSwitchValueASCII(
          ::switches::kAppModeOAuth2Token);
    }

    if (command_line->HasSwitch(::switches::kAppModeAuthCode)) {
      user_context_.auth_code = command_line->GetSwitchValueASCII(
          ::switches::kAppModeAuthCode);
    }

    DCHECK(!has_web_auth_cookies_);
    if (!user_context_.auth_code.empty()) {
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
  } else if (!user_context_.auth_code.empty()) {
    session_restore_strategy_ = OAuth2LoginManager::RESTORE_FROM_AUTH_CODE;
  } else {
    session_restore_strategy_ =
        OAuth2LoginManager::RESTORE_FROM_SAVED_OAUTH2_REFRESH_TOKEN;
  }
}


void LoginUtilsImpl::OnProfileCreated(
    const std::string& email,
    Profile* user_profile,
    Profile::CreateStatus status) {
  CHECK(user_profile);

  switch (status) {
    case Profile::CREATE_STATUS_CREATED:
      InitProfilePreferences(user_profile, email);
      break;
    case Profile::CREATE_STATUS_INITIALIZED:
      UserProfileInitialized(user_profile);
      break;
    case Profile::CREATE_STATUS_LOCAL_FAIL:
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::CREATE_STATUS_CANCELED:
    case Profile::MAX_CREATE_STATUS:
      NOTREACHED();
      break;
  }
}

void LoginUtilsImpl::UserProfileInitialized(Profile* user_profile) {
  BootTimesLoader* btl = BootTimesLoader::Get();
  btl->AddLoginTimeMarker("UserProfileGotten", false);

  if (using_oauth_) {
    // Transfer proxy authentication cache, cookies (optionally) and server
    // bound certs from the profile that was used for authentication.  This
    // profile contains cookies that auth extension should have already put in
    // place that will ensure that the newly created session is authenticated
    // for the websites that work with the used authentication schema.
    ProfileAuthData::Transfer(authenticator_->authentication_profile(),
                              user_profile,
                              has_web_auth_cookies_,  // transfer_cookies
                              base::Bind(
                                  &LoginUtilsImpl::CompleteProfileCreate,
                                  AsWeakPtr(),
                                  user_profile));
    return;
  }

  FinalizePrepareProfile(user_profile);
}

void LoginUtilsImpl::CompleteProfileCreate(Profile* user_profile) {
  RestoreAuthSession(user_profile, has_web_auth_cookies_);
  FinalizePrepareProfile(user_profile);
}

void LoginUtilsImpl::RestoreAuthSession(Profile* user_profile,
                                        bool restore_from_auth_cookies) {
  CHECK((authenticator_.get() && authenticator_->authentication_profile()) ||
        !restore_from_auth_cookies);

  if (chrome::IsRunningInForcedAppMode() ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kOobeSkipPostLogin))
    return;

  exit_after_session_restore_ = false;
  // Remove legacy OAuth1 token if we have one. If it's valid, we should already
  // have OAuth2 refresh token in TokenService that could be used to retrieve
  // all other tokens and user_context.
  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);
  login_manager->AddObserver(this);
  login_manager->RestoreSession(
      authenticator_.get() && authenticator_->authentication_profile()
          ? authenticator_->authentication_profile()->GetRequestContext()
          : NULL,
      session_restore_strategy_,
      oauth2_refresh_token_,
      user_context_.auth_code);
}

void LoginUtilsImpl::FinalizePrepareProfile(Profile* user_profile) {
  BootTimesLoader* btl = BootTimesLoader::Get();
  // Own TPM device if, for any reason, it has not been done in EULA
  // wizard screen.
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

  user_profile->OnLogin();

  // Send the notification before creating the browser so additional objects
  // that need the profile (e.g. the launcher) can be created first.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(user_profile));

  // Initialize RLZ only for primary user.
  if (UserManager::Get()->GetPrimaryUser() ==
      UserManager::Get()->GetUserByProfile(user_profile)) {
    InitRlzDelayed(user_profile);
  }
  // TODO(altimofeev): This pointer should probably never be NULL, but it looks
  // like LoginUtilsImpl::OnProfileCreated() may be getting called before
  // LoginUtilsImpl::PrepareProfile() has set |delegate_| when Chrome is killed
  // during shutdown in tests -- see http://crosbug.com/18269.  Replace this
  // 'if' statement with a CHECK(delegate_) once the underlying issue is
  // resolved.
  if (delegate_)
    delegate_->OnProfilePrepared(user_profile);
}

void LoginUtilsImpl::InitRlzDelayed(Profile* user_profile) {
#if defined(ENABLE_RLZ)
  if (!g_browser_process->local_state()->HasPrefPath(prefs::kRLZBrand)) {
    // Read brand code asynchronously from an OEM file and repost ourselves.
    google_util::chromeos::SetBrandFromFile(
        base::Bind(&LoginUtilsImpl::InitRlzDelayed, AsWeakPtr(), user_profile));
    return;
  }
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(false),
      FROM_HERE,
      base::Bind(&base::PathExists, GetRlzDisabledFlagPath()),
      base::Bind(&LoginUtilsImpl::InitRlz, AsWeakPtr(), user_profile));
#endif
}

void LoginUtilsImpl::InitRlz(Profile* user_profile, bool disabled) {
#if defined(ENABLE_RLZ)
  PrefService* local_state = g_browser_process->local_state();
  if (disabled) {
    // Empty brand code means an organic install (no RLZ pings are sent).
    google_util::chromeos::ClearBrandForCurrentSession();
  }
  if (disabled != local_state->GetBoolean(prefs::kRLZDisabled)) {
    // When switching to RLZ enabled/disabled state, clear all recorded events.
    RLZTracker::ClearRlzState();
    local_state->SetBoolean(prefs::kRLZDisabled, disabled);
  }
  // Init the RLZ library.
  int ping_delay = user_profile->GetPrefs()->GetInteger(
      first_run::GetPingDelayPrefName().c_str());
  // Negative ping delay means to send ping immediately after a first search is
  // recorded.
  RLZTracker::InitRlzFromProfileDelayed(
      user_profile, UserManager::Get()->IsCurrentUserNew(),
      ping_delay < 0, base::TimeDelta::FromMilliseconds(abs(ping_delay)));
  if (delegate_)
    delegate_->OnRlzInitialized(user_profile);
#endif
}

void LoginUtilsImpl::StartSignedInServices(Profile* user_profile) {
  // Fetch/Create the SigninManager - this will cause the TokenService to load
  // tokens for the currently signed-in user if the SigninManager hasn't
  // already been initialized.
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(user_profile);
  DCHECK(signin);
  // Make sure SigninManager is connected to our current user (this should
  // happen automatically because we set kGoogleServicesUsername in
  // OnProfileCreated()).
  DCHECK_EQ(UserManager::Get()->GetLoggedInUser()->display_email(),
            signin->GetAuthenticatedUsername());
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    // Notify the sync service that signin was successful. Note: Since the sync
    // service is lazy-initialized, we need to make sure it has been created.
    ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(user_profile);
    // We may not always have a passphrase (for example, on a restart after a
    // browser crash). Only notify the sync service if we have a passphrase,
    // so it can do any required re-encryption.
    if (!user_context_.password.empty() && sync_service) {
      GoogleServiceSigninSuccessDetails details(
          signin->GetAuthenticatedUsername(),
          user_context_.password);
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
          content::Source<Profile>(user_profile),
          content::Details<const GoogleServiceSigninSuccessDetails>(&details));
    }
  }
  user_context_.password.clear();
  user_context_.auth_code.clear();
}

void LoginUtilsImpl::CompleteOffTheRecordLogin(const GURL& start_url) {
  VLOG(1) << "Completing incognito login";

  // For guest session we ask session manager to restart Chrome with --bwsi
  // flag. We keep only some of the arguments of this process.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  CommandLine command_line(browser_command_line.GetProgram());
  std::string cmd_line_str = GetOffTheRecordCommandLine(start_url,
                                                        browser_command_line,
                                                        &command_line);

  RestartChrome(cmd_line_str);
}

void LoginUtilsImpl::SetFirstLoginPrefs(PrefService* prefs) {
  VLOG(1) << "Setting first login prefs";
  BootTimesLoader* btl = BootTimesLoader::Get();
  std::string locale = g_browser_process->GetApplicationLocale();

  // First, we'll set kLanguagePreloadEngines.
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  std::vector<std::string> input_method_ids;
  manager->GetInputMethodUtil()->GetFirstLoginInputMethodIds(
      locale, manager->GetCurrentInputMethod(), &input_method_ids);
  // Save the input methods in the user's preferences.
  StringPrefMember language_preload_engines;
  language_preload_engines.Init(prefs::kLanguagePreloadEngines,
                                prefs);
  language_preload_engines.SetValue(JoinString(input_method_ids, ','));
  btl->AddLoginTimeMarker("IMEStarted", false);

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
  language_preferred_languages.Init(prefs::kLanguagePreferredLanguages,
                                    prefs);
  language_preferred_languages.SetValue(JoinString(language_codes, ','));
}

scoped_refptr<Authenticator> LoginUtilsImpl::CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  // Screen locker needs new Authenticator instance each time.
  if (ScreenLocker::default_screen_locker()) {
    if (authenticator_.get())
      authenticator_->SetConsumer(NULL);
    authenticator_ = NULL;
  }

  if (authenticator_.get() == NULL) {
    authenticator_ = new ParallelAuthenticator(consumer);
  } else {
    // TODO(nkostylev): Fix this hack by improving Authenticator dependencies.
    authenticator_->SetConsumer(consumer);
  }
  return authenticator_;
}

void LoginUtilsImpl::RestoreAuthenticationSession(Profile* user_profile) {
  // We don't need to restore session for demo/guest/stub/public account users.
  if (!UserManager::Get()->IsUserLoggedIn() ||
      UserManager::Get()->IsLoggedInAsGuest() ||
      UserManager::Get()->IsLoggedInAsPublicAccount() ||
      UserManager::Get()->IsLoggedInAsDemoUser() ||
      UserManager::Get()->IsLoggedInAsStub()) {
    return;
  }

  if (!net::NetworkChangeNotifier::IsOffline()) {
    should_restore_auth_session_ = false;
    RestoreAuthSession(user_profile, false);
  } else {
    // Even if we're online we should wait till initial
    // OnConnectionTypeChanged() call. Otherwise starting fetchers too early may
    // end up canceling all request when initial network connection type is
    // processed. See http://crbug.com/121643.
    should_restore_auth_session_ = true;
  }
}

void LoginUtilsImpl::OnSessionRestoreStateChanged(
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

void LoginUtilsImpl::OnNewRefreshTokenAvaiable(Profile* user_profile) {
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
  // We need to exit cleanly in this case to make sure OAuth2 RT is actually
  // saved.
  chrome::ExitCleanly();
}

void LoginUtilsImpl::OnSessionAuthenticated(Profile* user_profile) {
  StartSignedInServices(user_profile);
}

void LoginUtilsImpl::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  Profile* user_profile = ProfileManager::GetDefaultProfile();
  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);

  if (type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      UserManager::Get()->IsUserLoggedIn()) {
    if (login_manager->state() ==
            OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS) {
      // If we come online for the first time after successful offline login,
      // we need to kick off OAuth token verification process again.
      login_manager->ContinueSessionRestore();
    } else if (should_restore_auth_session_) {
      should_restore_auth_session_ = false;
      RestoreAuthSession(user_profile, has_web_auth_cookies_);
    }
  }
}

void LoginUtilsImpl::AttemptExit(Profile* profile) {
  if (session_restore_strategy_ !=
      OAuth2LoginManager::RESTORE_FROM_COOKIE_JAR) {
    chrome::AttemptExit();
    return;
  }

  // We can't really quit if the session restore process that mints new
  // refresh token is still in progress.
  OAuth2LoginManager* login_manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile);
  if (login_manager->state() !=
          OAuth2LoginManager::SESSION_RESTORE_PREPARING &&
      login_manager->state() !=
          OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS) {
    chrome::AttemptExit();
    return;
  }

  LOG(WARNING) << "Attempting browser restart during session restore.";
  exit_after_session_restore_ = true;
}

// static
void LoginUtils::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFactoryResetRequested, false);
  registry->RegisterStringPref(prefs::kRLZBrand, std::string());
  registry->RegisterBooleanPref(prefs::kRLZDisabled, false);
}

// static
LoginUtils* LoginUtils::Get() {
  return LoginUtilsWrapper::GetInstance()->get();
}

// static
void LoginUtils::Set(LoginUtils* mock) {
  LoginUtilsWrapper::GetInstance()->reset(mock);
}

// static
bool LoginUtils::IsWhitelisted(const std::string& username) {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = false;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user)
    return true;
  return cros_settings->FindEmailInList(kAccountsPrefUsers, username);
}

}  // namespace chromeos
