// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_utils.h"

#include <algorithm>
#include <vector>

#include "ash/ash_switches.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/public/pref_member.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "cc/switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/oauth_login_manager.h"
#include "chrome/browser/chromeos/login/parallel_authenticator.h"
#include "chrome/browser/chromeos/login/profile_auth_data.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_util_chromeos.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/preconnect.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/network_configuration_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "googleurl/src/gurl.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/media_switches.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/views/corewm/corewm_switches.h"
#include "webkit/plugins/plugin_switches.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Affixes for Auth token received from ClientLogin request.
const char kAuthPrefix[] = "Auth=";
const char kAuthSuffix[] = "\n";

// Increase logging level for Guest mode to avoid LOG(INFO) messages in logs.
const char kGuestModeLoggingLevel[] = "1";

// Format of command line switch.
const char kSwitchFormatString[] = " --%s=\"%s\"";

// User name which is used in the Guest session.
const char kGuestUserName[] = "";


#if defined(ENABLE_RLZ)
// Flag file that disables RLZ tracking, when present.
const base::FilePath::CharType kRLZDisabledFlagName[] =
    FILE_PATH_LITERAL(".rlz_disabled");

base::FilePath GetRlzDisabledFlagPath() {
  return file_util::GetHomeDir().Append(kRLZDisabledFlagName);
}
#endif

}  // namespace

// Used to request a restart to switch to the guest mode.
class JobRestartRequest
    : public base::RefCountedThreadSafe<JobRestartRequest> {
 public:
  JobRestartRequest(int pid, const std::string& command_line)
      : pid_(pid),
        command_line_(command_line),
        local_state_(g_browser_process->local_state()) {
    AddRef();
    if (local_state_) {
      // XXX: normally this call must not be needed, however RestartJob
      // just kills us so settings may be lost. See http://crosbug.com/13102
      local_state_->CommitPendingWrite();
      timer_.Start(
          FROM_HERE, base::TimeDelta::FromSeconds(3), this,
          &JobRestartRequest::RestartJob);
      // Post task on FILE thread thus it occurs last on task queue, so it
      // would be executed after committing pending write on file thread.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&JobRestartRequest::RestartJob, this));
    } else {
      RestartJob();
    }
  }

 private:
  friend class base::RefCountedThreadSafe<JobRestartRequest>;

  ~JobRestartRequest() {}

  void RestartJob() {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      DBusThreadManager::Get()->GetSessionManagerClient()->RestartJob(
          pid_, command_line_);
    } else {
      // This function can be called on FILE thread. See PostTask in the
      // constructor.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&JobRestartRequest::RestartJob, this));
      MessageLoop::current()->AssertIdle();
    }
  }

  int pid_;
  std::string command_line_;
  PrefService* local_state_;
  base::OneShotTimer<JobRestartRequest> timer_;
};

class LoginUtilsImpl
    : public LoginUtils,
      public OAuthLoginManager::Delegate,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public content::NotificationObserver,
      public base::SupportsWeakPtr<LoginUtilsImpl> {
 public:
  LoginUtilsImpl()
      : using_oauth_(false),
        has_web_auth_cookies_(false),
        login_manager_(OAuthLoginManager::Create(this)),
        delegate_(NULL),
        job_restart_request_(NULL),
        should_restore_auth_session_(false),
        url_request_context_getter_(NULL) {
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
    // During tests, the browser_process may not be initialized yet causing
    // this to fail.
    if (g_browser_process) {
      registrar_.Add(
          this,
          chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED,
          content::Source<Profile>(ProfileManager::GetDefaultProfile()));
    }
  }

  virtual ~LoginUtilsImpl() {
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  }

  // LoginUtils implementation:
  virtual void DoBrowserLaunch(Profile* profile,
                               LoginDisplayHost* login_host) OVERRIDE;
  virtual void PrepareProfile(
      const std::string& username,
      const std::string& display_email,
      const std::string& password,
      bool using_oauth,
      bool has_cookies,
      LoginUtils::Delegate* delegate) OVERRIDE;
  virtual void DelegateDeleted(LoginUtils::Delegate* delegate) OVERRIDE;
  virtual void CompleteOffTheRecordLogin(const GURL& start_url) OVERRIDE;
  virtual void SetFirstLoginPrefs(PrefService* prefs) OVERRIDE;
  virtual scoped_refptr<Authenticator> CreateAuthenticator(
      LoginStatusConsumer* consumer) OVERRIDE;
  virtual void PrewarmAuthentication() OVERRIDE;
  virtual void RestoreAuthenticationSession(Profile* profile) OVERRIDE;
  virtual void StopBackgroundFetchers() OVERRIDE;
  virtual void InitRlzDelayed(Profile* user_profile) OVERRIDE;

  // OAuthLoginManager::Delegate overrides.
  virtual void OnCompletedMergeSession() OVERRIDE;
  virtual void OnCompletedAuthentication(Profile* user_profile) OVERRIDE;
  virtual void OnFoundStoredTokens() OVERRIDE;

  // net::NetworkChangeNotifier::ConnectionTypeObserver overrides.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  virtual std::string GetOffTheRecordCommandLine(
      const GURL& start_url,
      const CommandLine& base_command_line,
      CommandLine *command_line) OVERRIDE;

 private:
  // Restarts OAuth session authentication check.
  void KickStartAuthentication(Profile* profile);

  // Check user's profile for kApplicationLocale setting.
  void RespectLocalePreference(Profile* pref);

  // Callback for Profile::CREATE_STATUS_CREATED profile state.
  // Initializes basic preferences for newly created profile. Any other
  // early profile initialization that needs to happen before
  // ProfileManager::DoFinalInit() gets called is done here.
  void InitProfilePreferences(Profile* user_profile);

  // Callback for asynchronous profile creation.
  void OnProfileCreated(Profile* profile,
                        Profile::CreateStatus status);

  // Callback for Profile::CREATE_STATUS_INITIALIZED profile state.
  // Profile is created, extensions and promo resources are initialized.
  void UserProfileInitialized(Profile* user_profile);

  // Callback to resume profile creation after transferring auth data from
  // the authentication profile.
  void CompleteProfileCreate(Profile* user_profile);

  // Finalized profile preparation.
  void FinalizePrepareProfile(Profile* user_profile);

  // Restores GAIA auth cookies for the created user profile from OAuth2 token.
  void RestoreAuthSession(Profile* user_profile,
                          bool restore_from_auth_cookies);

  // Callback when managed mode preferences have been applied.
  void EnteredManagedMode(bool success);

  // Initializes RLZ. If |disabled| is true, RLZ pings are disabled.
  void InitRlz(Profile* user_profile, bool disabled);

  // Starts signing related services. Initiates TokenService token retrieval.
  void StartSignedInServices(Profile* profile);

  std::string password_;
  bool using_oauth_;
  // True if the authentication profile's cookie jar should contain
  // authentication cookies from the authentication extension log in flow.
  bool has_web_auth_cookies_;
  // Has to be scoped_refptr, see comment for CreateAuthenticator(...).
  scoped_refptr<Authenticator> authenticator_;
  scoped_ptr<OAuthLoginManager> login_manager_;

  // Delegate to be fired when the profile will be prepared.
  LoginUtils::Delegate* delegate_;

  // Used to restart Chrome to switch to the guest mode.
  JobRestartRequest* job_restart_request_;

  // True if should restore authentication session when notified about
  // online state change.
  bool should_restore_auth_session_;

  content::NotificationRegistrar registrar_;

  // This is set via a notification after the profile has initialized the
  // getter.
  net::URLRequestContextGetter* url_request_context_getter_;

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

  // Mark login host for deletion after browser starts.  This
  // guarantees that the message loop will be referenced by the
  // browser before it is dereferenced by the login host.
  if (login_host)
    login_host->OnSessionStart();
  UserManager::Get()->SessionStarted();
}

void LoginUtilsImpl::PrepareProfile(
    const std::string& username,
    const std::string& display_email,
    const std::string& password,
    bool using_oauth,
    bool has_cookies,
    LoginUtils::Delegate* delegate) {
  BootTimesLoader* btl = BootTimesLoader::Get();

  VLOG(1) << "Completing login for " << username;

  btl->AddLoginTimeMarker("StartSession-Start", false);
  DBusThreadManager::Get()->GetSessionManagerClient()->StartSession(
      username);
  btl->AddLoginTimeMarker("StartSession-End", false);

  btl->AddLoginTimeMarker("UserLoggedIn-Start", false);
  UserManager* user_manager = UserManager::Get();
  user_manager->UserLoggedIn(username, false);
  btl->AddLoginTimeMarker("UserLoggedIn-End", false);

  // Switch log file as soon as possible.
  if (base::chromeos::IsRunningOnChromeOS())
    logging::RedirectChromeLogging(*(CommandLine::ForCurrentProcess()));

  // Update user's displayed email.
  if (!display_email.empty())
    user_manager->SaveUserDisplayEmail(username, display_email);

  password_ = password;

  using_oauth_ = using_oauth;
  has_web_auth_cookies_ = has_cookies;
  delegate_ = delegate;

  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();

  // If this is an enterprise device and the user belongs to the enterprise
  // domain, then wait for a policy fetch before logging the user in. This
  // will delay Profile creation until the policy is fetched, so that features
  // controlled by policy (e.g. Sync, Startup tabs) only start after the
  // PrefService has the right values.
  // Profile creation is also resumed if the fetch attempt fails.
  bool wait_for_policy_fetch =
      using_oauth_ &&
      authenticator_.get() &&
      (connector->GetUserAffiliation(username) ==
           policy::USER_AFFILIATION_MANAGED);

  // Initialize user policy before the profile is created so the profile
  // initialization code sees the cached policy settings.
  connector->InitializeUserPolicy(username,
                                  user_manager->IsLoggedInAsPublicAccount(),
                                  wait_for_policy_fetch);

  // The default profile will have been changed because the ProfileManager
  // will process the notification that the UserManager sends out.
  ProfileManager::CreateDefaultProfileAsync(
      base::Bind(&LoginUtilsImpl::OnProfileCreated, AsWeakPtr()));

  if (wait_for_policy_fetch) {
    // Profile creation will block until user policy is fetched, which
    // requires the DeviceManagement token. Try to fetch it now.
    // TODO(atwilson): This is somewhat racy, as we are trying to fetch a
    // DMToken in parallel with loading the cached policy blob (there could
    // already be a DMToken in the cached policy). Once the legacy policy
    // framework is removed, this code can register a
    // CloudPolicyService::Observer to check whether the CloudPolicyClient was
    // able to register itself using the cached policy data, and then only
    // create a PolicyOAuthFetcher if the client is still unregistered
    // (http://crbug.com/143187).
    VLOG(1) << "Profile creation requires policy token, fetching now";
    login_manager_->RestorePolicyTokens(
        authenticator_->authentication_profile()->GetRequestContext());
  }
}

void LoginUtilsImpl::DelegateDeleted(LoginUtils::Delegate* delegate) {
  if (delegate_ == delegate)
    delegate_ = NULL;
}

void LoginUtilsImpl::InitProfilePreferences(Profile* user_profile) {
  if (UserManager::Get()->IsCurrentUserNew())
    SetFirstLoginPrefs(user_profile->GetPrefs());

  if (UserManager::Get()->IsLoggedInAsLocallyManagedUser()) {
    user_profile->GetPrefs()->SetBoolean(prefs::kProfileIsManaged, true);
  } else {
    // Make sure that the google service username is properly set (we do this
    // on every sign in, not just the first login, to deal with existing
    // profiles that might not have it set yet).
    StringPrefMember google_services_username;
    google_services_username.Init(prefs::kGoogleServicesUsername,
                                  user_profile->GetPrefs());
    google_services_username.SetValue(
        UserManager::Get()->GetLoggedInUser()->display_email());
  }

  // Make sure we flip every profile to not share proxies if the user hasn't
  // specified so explicitly.
  const PrefService::Preference* use_shared_proxies_pref =
      user_profile->GetPrefs()->FindPreference(prefs::kUseSharedProxies);
  if (use_shared_proxies_pref->IsDefaultValue())
    user_profile->GetPrefs()->SetBoolean(prefs::kUseSharedProxies, false);

  // Locally managed users do not have user policy initialized.
  if (!UserManager::Get()->IsLoggedInAsLocallyManagedUser()) {
    policy::NetworkConfigurationUpdater* network_configuration_updater =
        g_browser_process->browser_policy_connector()->
        GetNetworkConfigurationUpdater();
    if (network_configuration_updater)
      network_configuration_updater->OnUserPolicyInitialized();
  }

  RespectLocalePreference(user_profile);
}

void LoginUtilsImpl::OnProfileCreated(
    Profile* user_profile,
    Profile::CreateStatus status) {
  CHECK(user_profile);

  switch (status) {
    case Profile::CREATE_STATUS_INITIALIZED:
      UserProfileInitialized(user_profile);
      break;
    case Profile::CREATE_STATUS_CREATED:
      InitProfilePreferences(user_profile);
      break;
    case Profile::CREATE_STATUS_FAIL:
    default:
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

  if (UserManager::Get()->IsLoggedInAsLocallyManagedUser()) {
    // Apply managed mode first.
    ManagedMode::EnterManagedMode(
        user_profile,
        base::Bind(&LoginUtilsImpl::EnteredManagedMode, AsWeakPtr()));
  } else {
    FinalizePrepareProfile(user_profile);
  }
}

void LoginUtilsImpl::EnteredManagedMode(bool success) {
  // TODO(nkostylev): What if entering managed mode fails?
  FinalizePrepareProfile(ProfileManager::GetDefaultProfile());
}

void LoginUtilsImpl::CompleteProfileCreate(Profile* user_profile) {
  RestoreAuthSession(user_profile, has_web_auth_cookies_);
  FinalizePrepareProfile(user_profile);
}

void LoginUtilsImpl::RestoreAuthSession(Profile* user_profile,
                                        bool restore_from_auth_cookies) {
  CHECK((authenticator_ && authenticator_->authentication_profile()) ||
        !restore_from_auth_cookies);
  if (!login_manager_.get())
    return;

  UserManager::Get()->SetMergeSessionState(
      UserManager::MERGE_STATUS_IN_PROCESS);
  // Remove legacy OAuth1 token if we have one. If it's valid, we should already
  // have OAuth2 refresh token in TokenService that could be used to retrieve
  // all other tokens and credentials.
  login_manager_->RestoreSession(
      user_profile,
      authenticator_ && authenticator_->authentication_profile() ?
          authenticator_->authentication_profile()->GetRequestContext() :
          NULL,
      restore_from_auth_cookies);
}

void LoginUtilsImpl::FinalizePrepareProfile(Profile* user_profile) {
  BootTimesLoader* btl = BootTimesLoader::Get();
  // Own TPM device if, for any reason, it has not been done in EULA
  // wizard screen.
  CryptohomeLibrary* cryptohome = CrosLibrary::Get()->GetCryptohomeLibrary();
  btl->AddLoginTimeMarker("TPMOwn-Start", false);
  if (cryptohome->TpmIsEnabled() && !cryptohome->TpmIsBeingOwned()) {
    if (cryptohome->TpmIsOwned()) {
      cryptohome->TpmClearStoredPassword();
    } else {
      cryptohome->TpmCanAttemptOwnership();
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

  InitRlzDelayed(user_profile);

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
      base::Bind(&file_util::PathExists, GetRlzDisabledFlagPath()),
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
  // tokens for the currently signed-in user if the SigninManager hasn't already
  // been initialized.
  SigninManager* signin = SigninManagerFactory::GetForProfile(user_profile);
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
    if (!password_.empty() && sync_service) {
      GoogleServiceSigninSuccessDetails details(
          signin->GetAuthenticatedUsername(),
          password_);
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
          content::Source<Profile>(user_profile),
          content::Details<const GoogleServiceSigninSuccessDetails>(&details));
    }
  }
  password_.clear();
}

void LoginUtilsImpl::RespectLocalePreference(Profile* profile) {
  DCHECK(profile != NULL);
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs != NULL);
  if (g_browser_process == NULL)
    return;

  std::string pref_locale = prefs->GetString(prefs::kApplicationLocale);
  if (pref_locale.empty())
    pref_locale = prefs->GetString(prefs::kApplicationLocaleBackup);
  if (pref_locale.empty())
    pref_locale = g_browser_process->GetApplicationLocale();
  DCHECK(!pref_locale.empty());
  profile->ChangeAppLocale(pref_locale, Profile::APP_LOCALE_CHANGED_VIA_LOGIN);
  // Here we don't enable keyboard layouts. Input methods are set up when
  // the user first logs in. Then the user may customize the input methods.
  // Hence changing input methods here, just because the user's UI language
  // is different from the login screen UI language, is not desirable. Note
  // that input method preferences are synced, so users can use their
  // farovite input methods as soon as the preferences are synced.
  LanguageSwitchMenu::SwitchLanguage(pref_locale);
}

void LoginUtilsImpl::CompleteOffTheRecordLogin(const GURL& start_url) {
  VLOG(1) << "Completing incognito login";

  UserManager::Get()->GuestUserLoggedIn();

  // Session Manager may kill the chrome anytime after this point.
  // Write exit_cleanly and other stuff to the disk here.
  g_browser_process->EndSession();

  // For guest session we ask session manager to restart Chrome with --bwsi
  // flag. We keep only some of the arguments of this process.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  CommandLine command_line(browser_command_line.GetProgram());
  std::string cmd_line_str =
      GetOffTheRecordCommandLine(start_url,
                                 browser_command_line,
                                 &command_line);

  if (job_restart_request_) {
    NOTREACHED();
  }
  VLOG(1) << "Requesting a restart with PID " << getpid()
          << " and command line: " << cmd_line_str;
  job_restart_request_ = new JobRestartRequest(getpid(), cmd_line_str);
}

std::string LoginUtilsImpl::GetOffTheRecordCommandLine(
    const GURL& start_url,
    const CommandLine& base_command_line,
    CommandLine* command_line) {
  static const char* kForwardSwitches[] = {
      ::switches::kAllowWebUICompositing,
      ::switches::kDeviceManagementUrl,
      ::switches::kDisableAccelerated2dCanvas,
      ::switches::kDisableAcceleratedOverflowScroll,
      ::switches::kDisableAcceleratedPlugins,
      ::switches::kDisableAcceleratedVideoDecode,
      ::switches::kDisableEncryptedMedia,
      ::switches::kDisableForceCompositingMode,
      ::switches::kDisableGpuWatchdog,
      ::switches::kDisableLoginAnimations,
      ::switches::kDisableNonuniformGpuMemPolicy,
      ::switches::kDisableOobeAnimation,
      ::switches::kDisablePanelFitting,
      ::switches::kDisableThreadedCompositing,
      ::switches::kDisableSeccompFilterSandbox,
      ::switches::kDisableSeccompSandbox,
      ::switches::kEnableAcceleratedOverflowScroll,
      ::switches::kEnableCompositingForFixedPosition,
      ::switches::kEnableLogging,
      ::switches::kEnablePinch,
      ::switches::kEnableGestureTapHighlight,
      ::switches::kEnableViewport,
      ::switches::kForceDeviceScaleFactor,
      ::switches::kGpuStartupDialog,
      ::switches::kHasChromeOSKeyboard,
      ::switches::kLoginProfile,
      ::switches::kNaturalScrollDefault,
      ::switches::kNoSandbox,
      ::switches::kPpapiFlashArgs,
      ::switches::kPpapiFlashInProcess,
      ::switches::kPpapiFlashPath,
      ::switches::kPpapiFlashVersion,
      ::switches::kPpapiOutOfProcess,
      ::switches::kRendererStartupDialog,
#if defined(USE_XI2_MT)
      ::switches::kTouchCalibration,
#endif
      ::switches::kTouchDevices,
      ::switches::kTouchEvents,
      ::switches::kTouchOptimizedUI,
      ::switches::kOldCheckboxStyle,
      ::switches::kUIEnablePartialSwap,
      ::switches::kUIEnableThreadedCompositing,
      ::switches::kUIPrioritizeInGpuProcess,
#if defined(USE_CRAS)
      ::switches::kUseCras,
#endif
      ::switches::kUseGL,
      ::switches::kUserDataDir,
      ::switches::kUseExynosVda,
      ash::switches::kAshTouchHud,
      ash::switches::kAuraLegacyPowerButton,
      ash::switches::kAuraNoShadows,
      ash::switches::kAshEnableNewNetworkStatusArea,
      cc::switches::kDisableThreadedAnimation,
      cc::switches::kEnablePartialSwap,
      chromeos::switches::kDbusStub,
      gfx::switches::kEnableBrowserTextSubpixelPositioning,
      gfx::switches::kEnableWebkitTextSubpixelPositioning,
      views::corewm::switches::kWindowAnimationsDisabled,
  };
  command_line->CopySwitchesFrom(base_command_line,
                                 kForwardSwitches,
                                 arraysize(kForwardSwitches));
  command_line->AppendSwitch(::switches::kGuestSession);
  command_line->AppendSwitch(::switches::kIncognito);
  command_line->AppendSwitchASCII(::switches::kLoggingLevel,
                                 kGuestModeLoggingLevel);

  command_line->AppendSwitchASCII(::switches::kLoginUser, kGuestUserName);

  if (start_url.is_valid())
    command_line->AppendArg(start_url.spec());

  // Override the home page.
  command_line->AppendSwitchASCII(
      ::switches::kHomePage,
      GURL(chrome::kChromeUINewTabURL).spec());

  std::string cmd_line_str = command_line->GetCommandLineString();
  // Special workaround for the arguments that should be quoted.
  // Copying switches won't be needed when Guest mode won't need restart
  // http://crosbug.com/6924
  if (base_command_line.HasSwitch(::switches::kRegisterPepperPlugins)) {
    cmd_line_str += base::StringPrintf(
        kSwitchFormatString,
        ::switches::kRegisterPepperPlugins,
        base_command_line.GetSwitchValueNative(
            ::switches::kRegisterPepperPlugins).c_str());
  }

  return cmd_line_str;
}

void LoginUtilsImpl::SetFirstLoginPrefs(PrefService* prefs) {
  VLOG(1) << "Setting first login prefs";
  BootTimesLoader* btl = BootTimesLoader::Get();
  std::string locale = g_browser_process->GetApplicationLocale();

  // First, we'll set kLanguagePreloadEngines.
  input_method::InputMethodManager* manager =
      input_method::GetInputMethodManager();
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
    if (authenticator_)
      authenticator_->SetConsumer(NULL);
    authenticator_ = NULL;
  }

  if (authenticator_ == NULL) {
    authenticator_ = new ParallelAuthenticator(consumer);
  } else {
    // TODO(nkostylev): Fix this hack by improving Authenticator dependencies.
    authenticator_->SetConsumer(consumer);
  }
  return authenticator_;
}

// We use a special class for this so that it can be safely leaked if we
// never connect. At shutdown the order is not well defined, and it's possible
// for the infrastructure needed to unregister might be unstable and crash.
class WarmingObserver : public NetworkLibrary::NetworkManagerObserver,
                        public content::NotificationObserver {
 public:
  WarmingObserver()
      : url_request_context_getter_(NULL) {
    NetworkLibrary *netlib = CrosLibrary::Get()->GetNetworkLibrary();
    netlib->AddNetworkManagerObserver(this);
    // During tests, the browser_process may not be initialized yet causing
    // this to fail.
    if (g_browser_process) {
      registrar_.Add(
          this,
          chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED,
          content::Source<Profile>(ProfileManager::GetDefaultProfile()));
    }
  }

  virtual ~WarmingObserver() {}

  // If we're now connected, prewarm the auth url.
  virtual void OnNetworkManagerChanged(NetworkLibrary* netlib) OVERRIDE {
    if (netlib->Connected()) {
      const int kConnectionsNeeded = 1;
      chrome_browser_net::PreconnectOnUIThread(
          GURL(GaiaUrls::GetInstance()->client_login_url()),
          chrome_browser_net::UrlInfo::EARLY_LOAD_MOTIVATED,
          kConnectionsNeeded,
          url_request_context_getter_);
      netlib->RemoveNetworkManagerObserver(this);
      delete this;
    }
  }

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      url_request_context_getter_ = profile->GetRequestContext();
      registrar_.Remove(
          this,
          chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED,
          content::Source<Profile>(profile));

      break;
    }
    default:
      NOTREACHED();
  }
}
 private:
  net::URLRequestContextGetter* url_request_context_getter_;
  content::NotificationRegistrar registrar_;
};

void LoginUtilsImpl::PrewarmAuthentication() {
  NetworkLibrary *network = CrosLibrary::Get()->GetNetworkLibrary();
  if (network->Connected()) {
    const int kConnectionsNeeded = 1;
    chrome_browser_net::PreconnectOnUIThread(
        GURL(GaiaUrls::GetInstance()->client_login_url()),
        chrome_browser_net::UrlInfo::EARLY_LOAD_MOTIVATED,
        kConnectionsNeeded,
        url_request_context_getter_);
  } else {
    new WarmingObserver();
  }
}

void LoginUtilsImpl::RestoreAuthenticationSession(Profile* user_profile) {
  // We don't need to restore session for demo/guest users.
  if (!UserManager::Get()->IsUserLoggedIn() ||
      UserManager::Get()->IsLoggedInAsGuest() ||
      UserManager::Get()->IsLoggedInAsDemoUser()) {
    return;
  }

  if (!net::NetworkChangeNotifier::IsOffline()) {
    should_restore_auth_session_ = false;
    RestoreAuthSession(user_profile, false);
  } else {
    // Even if we're online we should wait till initial
    // OnConnectionTypeChanged() call. Otherwise starting fetchers too early may
    // end up cancelling all request when initial network connection type is
    // processed. See http://crbug.com/121643.
    should_restore_auth_session_ = true;
  }
}

void LoginUtilsImpl::StopBackgroundFetchers() {
  login_manager_.reset();
}

void LoginUtilsImpl::OnCompletedAuthentication(Profile* user_profile) {
  StartSignedInServices(user_profile);
}

void LoginUtilsImpl::OnCompletedMergeSession() {
  UserManager::Get()->SetMergeSessionState(UserManager::MERGE_STATUS_DONE);
}

void LoginUtilsImpl::OnFoundStoredTokens() {
  // We don't need authenticator instance any more since its cookie jar
  // is not going to needed to mint OAuth tokens. Reset it so that
  // ScreenLocker would create a separate instance.
  authenticator_ = NULL;
}

void LoginUtilsImpl::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (!login_manager_.get())
    return;

  if (type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      UserManager::Get()->IsUserLoggedIn()) {
    if (login_manager_->state() ==
            OAuthLoginManager::SESSION_RESTORE_IN_PROGRESS) {
      // If we come online for the first time after successful offline login,
      // we need to kick off OAuth token verification process again.
      login_manager_->ContinueSessionRestore();
    } else if (should_restore_auth_session_) {
      should_restore_auth_session_ = false;
      Profile* user_profile = ProfileManager::GetDefaultProfile();
      RestoreAuthSession(user_profile, has_web_auth_cookies_);
    }
  }
}

void LoginUtilsImpl::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      url_request_context_getter_ = profile->GetRequestContext();
      registrar_.Remove(
          this,
          chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED,
          content::Source<Profile>(profile));
      break;
    }
    default:
      NOTREACHED();
  }
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
