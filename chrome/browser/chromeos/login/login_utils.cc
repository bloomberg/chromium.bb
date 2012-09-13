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
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/prefs/pref_member.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/oauth1_token_fetcher.h"
#include "chrome/browser/chromeos/login/oauth_login_verifier.h"
#include "chrome/browser/chromeos/login/parallel_authenticator.h"
#include "chrome/browser/chromeos/login/policy_oauth_fetcher.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/preconnect.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/user_cloud_policy_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
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
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "googleurl/src/gurl.h"
#include "media/base/media_switches.h"
#include "net/base/network_change_notifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"
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

class InitializeCookieMonsterHelper {
 public:
  explicit InitializeCookieMonsterHelper(
      net::URLRequestContextGetter* new_context)
          : ALLOW_THIS_IN_INITIALIZER_LIST(callback_(base::Bind(
              &InitializeCookieMonsterHelper::InitializeCookieMonster,
              base::Unretained(this)))),
            new_context_(new_context) {
  }

  const net::CookieMonster::GetCookieListCallback& callback() const {
    return callback_;
  }

 private:
  void InitializeCookieMonster(const net::CookieList& cookies) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::CookieStore* new_store =
        new_context_->GetURLRequestContext()->cookie_store();
    net::CookieMonster* new_monster = new_store->GetCookieMonster();

    if (!new_monster->InitializeFrom(cookies))
      LOG(WARNING) << "Failed initial cookie transfer.";
  }

  net::CookieMonster::GetCookieListCallback callback_;
  scoped_refptr<net::URLRequestContextGetter> new_context_;

  DISALLOW_COPY_AND_ASSIGN(InitializeCookieMonsterHelper);
};

// Transfers initial set of Profile cookies from the default profile.
void TransferDefaultCookiesOnIOThread(
    net::URLRequestContextGetter* auth_context,
    net::URLRequestContextGetter* new_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieStore* default_store =
      auth_context->GetURLRequestContext()->cookie_store();

  InitializeCookieMonsterHelper helper(new_context);
  net::CookieMonster* default_monster = default_store->GetCookieMonster();
  default_monster->SetKeepExpiredCookies();
  default_monster->GetAllCookiesAsync(helper.callback());
}

void TransferDefaultAuthCacheOnIOThread(
    net::URLRequestContextGetter* auth_context,
      net::URLRequestContextGetter* new_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::HttpAuthCache* new_cache = new_context->GetURLRequestContext()->
      http_transaction_factory()->GetSession()->http_auth_cache();
  new_cache->UpdateAllFrom(*auth_context->GetURLRequestContext()->
      http_transaction_factory()->GetSession()->http_auth_cache());
}

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
      public OAuth1TokenFetcher::Delegate,
      public OAuthLoginVerifier::Delegate,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public content::NotificationObserver,
      public base::SupportsWeakPtr<LoginUtilsImpl> {
 public:
  LoginUtilsImpl()
      : pending_requests_(false),
        using_oauth_(false),
        has_cookies_(false),
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
      bool pending_requests,
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
  virtual void StartTokenServices(Profile* user_profile) OVERRIDE;
  virtual void StartSignedInServices(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials) OVERRIDE;
  virtual void TransferDefaultCookies(Profile* default_profile,
                                      Profile* new_profile) OVERRIDE;
  virtual void TransferDefaultAuthCache(Profile* default_profile,
                                        Profile* new_profile) OVERRIDE;
  virtual void StopBackgroundFetchers() OVERRIDE;

  // OAuth1TokenFetcher::Delegate overrides.
  void OnOAuth1AccessTokenAvailable(const std::string& token,
                                    const std::string& secret) OVERRIDE;
  void OnOAuth1AccessTokenFetchFailed() OVERRIDE;

  // OAuthLoginVerifier::Delegate overrides.
  virtual void OnOAuthVerificationSucceeded(const std::string& user_name,
                                            const std::string& sid,
                                            const std::string& lsid,
                                            const std::string& auth) OVERRIDE;
  virtual void OnOAuthVerificationFailed(const std::string& user_name) OVERRIDE;

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
      CommandLine *command_line);

 private:
  // Restarts OAuth session authentication check.
  void KickStartAuthentication(Profile* profile);

  // Reads OAuth1 token from user profile's prefs.
  bool ReadOAuth1AccessToken(Profile* user_profile,
                             std::string* token,
                             std::string* secret);

  // Stores OAuth1 token + secret in profile's prefs.
  void StoreOAuth1AccessToken(Profile* user_profile,
                              const std::string& token,
                              const std::string& secret);

  // Verifies OAuth1 token by doing OAuthLogin and fetching credentials.
  void VerifyOAuth1AccessToken(Profile* user_profile,
                               const std::string& token,
                               const std::string& secret);

  // Fetch all secondary (OAuth2) tokens given OAuth1 access |token| and
  // |secret|.
  void FetchSecondaryTokens(Profile* offrecord_profile,
                            const std::string& token,
                            const std::string& secret);

  // Fetch user credentials (sid/lsid) given OAuth1 access |token| and |secret|.
  void FetchCredentials(Profile* user_profile,
                        const std::string& token,
                        const std::string& secret);

  // Fetch enterprise policy OAuth2 given OAuth1 access |token| and |secret|.
  void FetchPolicyToken(Profile* offrecord_profile,
                        const std::string& token,
                        const std::string& secret);

  // Check user's profile for kApplicationLocale setting.
  void RespectLocalePreference(Profile* pref);

  // Callback for asynchronous profile creation.
  void OnProfileCreated(Profile* profile,
                        Profile::CreateStatus status);

  std::string password_;
  bool pending_requests_;
  bool using_oauth_;
  bool has_cookies_;
  // Has to be scoped_refptr, see comment for CreateAuthenticator(...).
  scoped_refptr<Authenticator> authenticator_;
  scoped_ptr<PolicyOAuthFetcher> policy_oauth_fetcher_;
  scoped_ptr<OAuth1TokenFetcher> oauth1_token_fetcher_;
  scoped_ptr<OAuthLoginVerifier> oauth_login_verifier_;

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

  if (login_host)
    login_host->SetStatusAreaVisible(true);

  BootTimesLoader::Get()->AddLoginTimeMarker("BrowserLaunched", false);

  VLOG(1) << "Launching browser...";
  StartupBrowserCreator browser_creator;
  int return_code;
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  browser_creator.LaunchBrowser(*CommandLine::ForCurrentProcess(),
                                profile,
                                FilePath(),
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
    bool pending_requests,
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

  pending_requests_ = pending_requests;
  using_oauth_ = using_oauth;
  has_cookies_ = has_cookies;
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
  connector->InitializeUserPolicy(username, wait_for_policy_fetch);

  // The default profile will have been changed because the ProfileManager
  // will process the notification that the UserManager sends out.
  ProfileManager::CreateDefaultProfileAsync(
      base::Bind(&LoginUtilsImpl::OnProfileCreated, AsWeakPtr()));

  // The default profile is only partially initialized at this point.
  // Setup the UserCloudPolicyManager so profile initialization can complete.
  Profile* user_profile = ProfileManager::GetDefaultProfile();

  // Initialize the new cloud policy framework, if enabled.
  if (user_profile->GetUserCloudPolicyManager()) {
    user_profile->GetUserCloudPolicyManager()->Initialize(
        g_browser_process->local_state(),
        connector->device_management_service(),
        connector->GetUserAffiliation(username));
  }

  if (wait_for_policy_fetch) {
    // Profile creation will block until user policy is fetched, which
    // requires the DeviceManagement token. Try to fetch it now.
    VLOG(1) << "Profile creation requires policy token, fetching now";
    policy_oauth_fetcher_.reset(
        new PolicyOAuthFetcher(authenticator_->authentication_profile()));
    policy_oauth_fetcher_->Start();
  }
}

void LoginUtilsImpl::DelegateDeleted(LoginUtils::Delegate* delegate) {
  if (delegate_ == delegate)
    delegate_ = NULL;
}

void LoginUtilsImpl::OnProfileCreated(
    Profile* user_profile,
    Profile::CreateStatus status) {
  CHECK(user_profile);
  switch (status) {
    case Profile::CREATE_STATUS_INITIALIZED:
      break;
    case Profile::CREATE_STATUS_CREATED: {
      if (UserManager::Get()->IsCurrentUserNew())
        SetFirstLoginPrefs(user_profile->GetPrefs());
      // Make sure that the google service username is properly set (we do this
      // on every sign in, not just the first login, to deal with existing
      // profiles that might not have it set yet).
      StringPrefMember google_services_username;
      google_services_username.Init(prefs::kGoogleServicesUsername,
                                    user_profile->GetPrefs(), NULL);
      google_services_username.SetValue(
          UserManager::Get()->GetLoggedInUser().display_email());
      // Make sure we flip every profile to not share proxies if the user hasn't
      // specified so explicitly.
      const PrefService::Preference* use_shared_proxies_pref =
          user_profile->GetPrefs()->FindPreference(prefs::kUseSharedProxies);
      if (use_shared_proxies_pref->IsDefaultValue())
        user_profile->GetPrefs()->SetBoolean(prefs::kUseSharedProxies, false);
      RespectLocalePreference(user_profile);
      return;
    }
    case Profile::CREATE_STATUS_FAIL:
    default:
      NOTREACHED();
      return;
  }

  BootTimesLoader* btl = BootTimesLoader::Get();
  btl->AddLoginTimeMarker("UserProfileGotten", false);

  if (using_oauth_) {
    // Reuse the access token fetched by the PolicyOAuthFetcher, if it was
    // used to fetch policies before Profile creation.
    if (policy_oauth_fetcher_.get() &&
        !policy_oauth_fetcher_->oauth1_token().empty()) {
      VLOG(1) << "Resuming profile creation after fetching policy token";
      StoreOAuth1AccessToken(user_profile,
                             policy_oauth_fetcher_->oauth1_token(),
                             policy_oauth_fetcher_->oauth1_secret());
    }

    // Transfer cookies when user signs in using extension.
    if (has_cookies_) {
      // Transfer cookies from the profile that was used for authentication.
      // This profile contains cookies that auth extension should have already
      // put in place that will ensure that the newly created session is
      // authenticated for the websites that work with the used authentication
      // schema.
      TransferDefaultCookies(authenticator_->authentication_profile(),
                             user_profile);
    }
    // Transfer proxy authentication cache.
    TransferDefaultAuthCache(authenticator_->authentication_profile(),
                             user_profile);
    std::string oauth1_token;
    std::string oauth1_secret;
    if (ReadOAuth1AccessToken(user_profile, &oauth1_token, &oauth1_secret) ||
        !has_cookies_) {
      // Verify OAuth access token when we find it in the profile and always if
      // if we don't have cookies.
      // TODO(xiyuan): Change back to use authenticator to verify token when
      // we support Gaia in lock screen.
      VerifyOAuth1AccessToken(user_profile, oauth1_token, oauth1_secret);
    } else {
      // If we don't have it, fetch OAuth1 access token.
      // Once we get that, we will kick off individual requests for OAuth2
      // tokens for all our services.
      // Use off-the-record profile that was used for this step. It should
      // already contain all needed cookies that will let us skip GAIA's user
      // authentication UI.
      //
      // TODO(rickcam) We should use an isolated App here.
      oauth1_token_fetcher_.reset(
          new OAuth1TokenFetcher(this,
                                 authenticator_->authentication_profile()));
      oauth1_token_fetcher_->Start();
    }
  }

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

  // TODO(altimofeev): This pointer should probably never be NULL, but it looks
  // like LoginUtilsImpl::OnProfileCreated() may be getting called before
  // LoginUtilsImpl::PrepareProfile() has set |delegate_| when Chrome is killed
  // during shutdown in tests -- see http://crosbug.com/18269.  Replace this
  // 'if' statement with a CHECK(delegate_) once the underlying issue is
  // resolved.
  if (delegate_)
    delegate_->OnProfilePrepared(user_profile);
}

void LoginUtilsImpl::StartTokenServices(Profile* user_profile) {
  std::string oauth1_token;
  std::string oauth1_secret;
  if (!ReadOAuth1AccessToken(user_profile, &oauth1_token, &oauth1_secret))
    return;

  FetchSecondaryTokens(user_profile->GetOffTheRecordProfile(),
                       oauth1_token, oauth1_secret);
}

void LoginUtilsImpl::StartSignedInServices(
    Profile* user_profile,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // Fetch/Create the SigninManager - this will cause the TokenService to load
  // tokens for the currently signed-in user if the SigninManager hasn't already
  // been initialized.
  SigninManager* signin = SigninManagerFactory::GetForProfile(user_profile);
  DCHECK(signin);
  // Make sure SigninManager is connected to our current user (this should
  // happen automatically because we set kGoogleServicesUsername in
  // OnProfileCreated()).
  DCHECK_EQ(UserManager::Get()->GetLoggedInUser().display_email(),
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
  TokenService* token_service =
      TokenServiceFactory::GetForProfile(user_profile);
  token_service->UpdateCredentials(credentials);
  if (token_service->AreCredentialsValid())
    token_service->StartFetchingTokens();
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
      ::switches::kCompressSystemFeedback,
      ::switches::kDeviceManagementUrl,
      ::switches::kForceDeviceScaleFactor,
      ::switches::kDisableAccelerated2dCanvas,
      ::switches::kDisableAcceleratedPlugins,
      ::switches::kDisableAcceleratedVideoDecode,
      ::switches::kDisableGpuWatchdog,
      ::switches::kDisableLoginAnimations,
      ::switches::kDisableOobeAnimation,
      ::switches::kDisableSeccompFilterSandbox,
      ::switches::kDisableSeccompSandbox,
      ::switches::kDisableThreadedAnimation,
      ::switches::kEnableCompositingForFixedPosition,
      ::switches::kEnableGView,
      ::switches::kEnableLogging,
      ::switches::kEnablePartialSwap,
      ::switches::kEnableUIReleaseFrontSurface,
      ::switches::kEnablePinch,
      ::switches::kEnableGestureTapHighlight,
      ::switches::kEnableSmoothScrolling,
      ::switches::kEnableThreadedCompositing,
      ::switches::kEnableTouchCalibration,
      ::switches::kEnableTouchEvents,
      ::switches::kEnableViewport,
      ::switches::kDisableThreadedCompositing,
      ::switches::kForceCompositingMode,
      ::switches::kGpuStartupDialog,
      ::switches::kLoginProfile,
      ::switches::kScrollPixels,
      ::switches::kNoFirstRun,
      ::switches::kNoSandbox,
      ::switches::kPpapiFlashArgs,
      ::switches::kPpapiFlashInProcess,
      ::switches::kPpapiFlashPath,
      ::switches::kPpapiFlashVersion,
      ::switches::kRendererStartupDialog,
      ::switches::kFlingTapSuppressMaxDown,
      ::switches::kFlingTapSuppressMaxGap,
      ::switches::kTouchDevices,
      ::switches::kTouchOptimizedUI,
      ::switches::kOldCheckboxStyle,
      ash::switches::kAshTouchHud,
      ash::switches::kAshWindowAnimationsDisabled,
      ash::switches::kAuraLegacyPowerButton,
      ash::switches::kAuraNoShadows,
      ash::switches::kAshNotifyDisabled,
      ::switches::kUIEnablePartialSwap,
      ::switches::kUIPrioritizeInGpuProcess,
#if defined(USE_CRAS)
      ::switches::kUseCras,
#endif
      ::switches::kUseGL,
      ::switches::kUserDataDir,
      chromeos::switches::kDbusStub,
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

  // Override the value of the homepage that is set in first run mode.
  // TODO(altimofeev): extend action of the |kNoFirstRun| to cover this case.
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
      input_method::InputMethodManager::GetInstance();
  std::vector<std::string> input_method_ids;
  manager->GetInputMethodUtil()->GetFirstLoginInputMethodIds(
      locale, manager->GetCurrentInputMethod(), &input_method_ids);
  // Save the input methods in the user's preferences.
  StringPrefMember language_preload_engines;
  language_preload_engines.Init(prefs::kLanguagePreloadEngines,
                                prefs, NULL);
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
                                    prefs, NULL);
  language_preferred_languages.SetValue(JoinString(language_codes, ','));
}

scoped_refptr<Authenticator> LoginUtilsImpl::CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  // Screen locker needs new Authenticator instance each time.
  if (ScreenLocker::default_screen_locker())
    authenticator_ = NULL;

  if (authenticator_ == NULL)
    authenticator_ = new ParallelAuthenticator(consumer);
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
  virtual void OnNetworkManagerChanged(NetworkLibrary* netlib) {
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
    KickStartAuthentication(user_profile);
  } else {
    // Even if we're online we should wait till initial
    // OnConnectionTypeChanged() call. Otherwise starting fetchers too early may
    // end up cancelling all request when initial network connection type is
    // processed. See http://crbug.com/121643.
    should_restore_auth_session_ = true;
  }
}

void LoginUtilsImpl::KickStartAuthentication(Profile* user_profile) {
  std::string oauth1_token;
  std::string oauth1_secret;
  if (ReadOAuth1AccessToken(user_profile, &oauth1_token, &oauth1_secret))
    VerifyOAuth1AccessToken(user_profile, oauth1_token, oauth1_secret);
}

void LoginUtilsImpl::TransferDefaultCookies(Profile* default_profile,
                                            Profile* profile) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TransferDefaultCookiesOnIOThread,
                 make_scoped_refptr(default_profile->GetRequestContext()),
                 make_scoped_refptr(profile->GetRequestContext())));
}

void LoginUtilsImpl::TransferDefaultAuthCache(Profile* default_profile,
                                              Profile* profile) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TransferDefaultAuthCacheOnIOThread,
                 make_scoped_refptr(default_profile->GetRequestContext()),
                 make_scoped_refptr(profile->GetRequestContext())));
}

void LoginUtilsImpl::StopBackgroundFetchers() {
  policy_oauth_fetcher_.reset();
  oauth1_token_fetcher_.reset();
  oauth_login_verifier_.reset();
}

void LoginUtilsImpl::FetchSecondaryTokens(Profile* offrecord_profile,
                                          const std::string& token,
                                          const std::string& secret) {
  FetchPolicyToken(offrecord_profile, token, secret);
  // TODO(rickcam, zelidrag): Wire TokenService there when it becomes
  // capable of handling OAuth1 tokens directly.
}

bool LoginUtilsImpl::ReadOAuth1AccessToken(Profile* user_profile,
                                           std::string* token,
                                           std::string* secret) {
  // Skip reading oauth token if user does not have a valid status.
  if (UserManager::Get()->IsUserLoggedIn() &&
      UserManager::Get()->GetLoggedInUser().oauth_token_status() !=
      User::OAUTH_TOKEN_STATUS_VALID) {
    return false;
  }

  PrefService* pref_service = user_profile->GetPrefs();
  std::string encoded_token = pref_service->GetString(prefs::kOAuth1Token);
  std::string encoded_secret = pref_service->GetString(prefs::kOAuth1Secret);
  if (!encoded_token.length() || !encoded_secret.length())
    return false;

  std::string decoded_token =
      CrosLibrary::Get()->GetCertLibrary()->DecryptToken(encoded_token);
  std::string decoded_secret =
      CrosLibrary::Get()->GetCertLibrary()->DecryptToken(encoded_secret);
  if (!decoded_token.length() || !decoded_secret.length())
    return false;

  *token = decoded_token;
  *secret = decoded_secret;
  return true;
}

void LoginUtilsImpl::StoreOAuth1AccessToken(Profile* user_profile,
                                            const std::string& token,
                                            const std::string& secret) {
  // First store OAuth1 token + service for the current user profile...
  std::string encrypted_token =
      CrosLibrary::Get()->GetCertLibrary()->EncryptToken(token);
  std::string encrypted_secret =
      CrosLibrary::Get()->GetCertLibrary()->EncryptToken(secret);
  PrefService* pref_service = user_profile->GetPrefs();
  if (!encrypted_token.empty() && !encrypted_secret.empty()) {
    pref_service->SetString(prefs::kOAuth1Token, encrypted_token);
    pref_service->SetString(prefs::kOAuth1Secret, encrypted_secret);

    // ...then record the presence of valid OAuth token for this account in
    // local state as well.
    UserManager::Get()->SaveUserOAuthStatus(
        UserManager::Get()->GetLoggedInUser().email(),
        User::OAUTH_TOKEN_STATUS_VALID);
  } else {
    LOG(WARNING) << "Failed to get OAuth1 token/secret encrypted.";
    // Set the OAuth status invalid so that the user will go through full
    // GAIA login next time.
    UserManager::Get()->SaveUserOAuthStatus(
        UserManager::Get()->GetLoggedInUser().email(),
        User::OAUTH_TOKEN_STATUS_INVALID);
  }
}

void LoginUtilsImpl::VerifyOAuth1AccessToken(Profile* user_profile,
                                             const std::string& token,
                                             const std::string& secret) {
  // Kick off verification of OAuth1 access token (via OAuthLogin), this should
  // let us fetch credentials that will be used to initialize sync engine.
  FetchCredentials(user_profile, token, secret);

  FetchSecondaryTokens(user_profile->GetOffTheRecordProfile(), token, secret);
}

void LoginUtilsImpl::FetchCredentials(Profile* user_profile,
                                      const std::string& token,
                                      const std::string& secret) {
  oauth_login_verifier_.reset(new OAuthLoginVerifier(
      this, user_profile, token, secret,
      UserManager::Get()->GetLoggedInUser().email()));
  oauth_login_verifier_->StartOAuthVerification();
}


void LoginUtilsImpl::FetchPolicyToken(Profile* offrecord_profile,
                                      const std::string& token,
                                      const std::string& secret) {
  // Fetch dm service token now, if it hasn't been fetched yet.
  if (!policy_oauth_fetcher_.get() || policy_oauth_fetcher_->failed()) {
    // Get the default system profile to use with the policy fetching. If there
    // is no |authenticator_| profile, manually load default system profile.
    // Otherwise, just use |authenticator_|'s profile.
    Profile* profile = NULL;
    if (authenticator_)
      profile = authenticator_->authentication_profile();

    if (!profile) {
      FilePath user_data_dir;
      PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      // Temporarily allow until fix: http://crosbug.com/30391.
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      profile = profile_manager->GetProfile(user_data_dir)->
          GetOffTheRecordProfile();
    }

    // Trigger oauth token fetch for user policy.
    policy_oauth_fetcher_.reset(new PolicyOAuthFetcher(profile, token, secret));
    policy_oauth_fetcher_->Start();
  }

  // TODO(zelidrag): We should add initialization of other services somewhere
  // here as well. This could be handled with TokenService class once it is
  // ready to handle OAuth tokens.

  // We don't need authenticator instance any more, reset it so that
  // ScreenLocker would create a separate instance.
  // TODO(nkostylev): There's a potential race if SL would be created before
  // OAuth tokens are fetched. It would use incorrect Authenticator instance.
  authenticator_ = NULL;
}

void LoginUtilsImpl::OnOAuthVerificationFailed(const std::string& user_name) {
  UserManager::Get()->SaveUserOAuthStatus(user_name,
                                          User::OAUTH_TOKEN_STATUS_INVALID);
}

void LoginUtilsImpl::OnOAuth1AccessTokenAvailable(const std::string& token,
                                                  const std::string& secret) {
  Profile* user_profile = ProfileManager::GetDefaultProfile();
  StoreOAuth1AccessToken(user_profile, token, secret);

  // Verify OAuth1 token by doing OAuthLogin and fetching credentials.
  VerifyOAuth1AccessToken(user_profile, token, secret);
}

void LoginUtilsImpl::OnOAuth1AccessTokenFetchFailed() {
  // TODO(kochi): Show failure notification UI here?
  LOG(ERROR) << "Failed to fetch OAuth1 access token.";
}

void LoginUtilsImpl::OnOAuthVerificationSucceeded(
    const std::string& user_name, const std::string& sid,
    const std::string& lsid, const std::string& auth) {
  // Kick off sync engine.
  GaiaAuthConsumer::ClientLoginResult credentials(sid, lsid, auth,
                                                  std::string());
  StartSignedInServices(ProfileManager::GetDefaultProfile(), credentials);
}


void LoginUtilsImpl::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      UserManager::Get()->IsUserLoggedIn()) {
    if (oauth_login_verifier_.get() &&
        !oauth_login_verifier_->is_done()) {
      // If we come online for the first time after successful offline login,
      // we need to kick off OAuth token verification process again.
      oauth_login_verifier_->ContinueVerification();
    } else if (should_restore_auth_session_) {
      should_restore_auth_session_ = false;
      Profile* user_profile = ProfileManager::GetDefaultProfile();
      KickStartAuthentication(user_profile);
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
