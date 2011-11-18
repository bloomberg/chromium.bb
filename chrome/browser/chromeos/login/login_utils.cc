// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_utils.h"

#include <vector>

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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/session_manager_client.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/cookie_fetcher.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/parallel_authenticator.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/gaia/gaia_oauth_consumer.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/net/preconnect.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_store.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/gl/gl_switches.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// OAuth token verification retry count.
const int kMaxOAuthTokenVerificationAttemptCount = 5;
// OAuth token verification retry delay.
const int kOAuthVerificationRestartDelay = 10000;  // ms

// Affixes for Auth token received from ClientLogin request.
const char kAuthPrefix[] = "Auth=";
const char kAuthSuffix[] = "\n";

// Increase logging level for Guest mode to avoid LOG(INFO) messages in logs.
const char kGuestModeLoggingLevel[] = "1";

// Format of command line switch.
const char kSwitchFormatString[] = " --%s=\"%s\"";

// User name which is used in the Guest session.
const char kGuestUserName[] = "";

// The service scope of the OAuth v2 token that ChromeOS login will be
// requesting.
// TODO(zelidrag): Figure out if we need to add more services here.
const char kServiceScopeChromeOS[] =
    "https://www.googleapis.com/auth/chromesync";

const char kServiceScopeChromeOSDeviceManagement[] =
    "https://www.googleapis.com/auth/chromeosdevicemanagement";
}  // namespace

// Transfers initial set of Profile cookies from the default profile.
class TransferDefaultCookiesOnIOThreadTask : public Task {
 public:
  TransferDefaultCookiesOnIOThreadTask(
      net::URLRequestContextGetter* auth_context,
      net::URLRequestContextGetter* new_context)
          : auth_context_(auth_context),
            new_context_(new_context) {}
  virtual ~TransferDefaultCookiesOnIOThreadTask() {}

  // Task override.
  virtual void Run() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::CookieStore* default_store =
        auth_context_->GetURLRequestContext()->cookie_store();
    net::CookieMonster* default_monster = default_store->GetCookieMonster();
    default_monster->SetKeepExpiredCookies();
    default_monster->GetAllCookiesAsync(
        base::Bind(
            &TransferDefaultCookiesOnIOThreadTask::InitializeCookieMonster,
            base::Unretained(this)));
  }

  void InitializeCookieMonster(const net::CookieList& cookies) {
     DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
     net::CookieStore* new_store =
         new_context_->GetURLRequestContext()->cookie_store();
     net::CookieMonster* new_monster = new_store->GetCookieMonster();

    if (!new_monster->InitializeFrom(cookies)) {
      LOG(WARNING) << "Failed initial cookie transfer.";
    }
  }

 private:
  net::URLRequestContextGetter* auth_context_;
  net::URLRequestContextGetter* new_context_;

  DISALLOW_COPY_AND_ASSIGN(TransferDefaultCookiesOnIOThreadTask);
};

// Transfers initial HTTP proxy authentication from the default profile.
class TransferDefaultAuthCacheOnIOThreadTask : public Task {
 public:
  TransferDefaultAuthCacheOnIOThreadTask(
      net::URLRequestContextGetter* auth_context,
      net::URLRequestContextGetter* new_context)
          : auth_context_(auth_context),
            new_context_(new_context) {}
  virtual ~TransferDefaultAuthCacheOnIOThreadTask() {}

  // Task override.
  virtual void Run() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::HttpAuthCache* new_cache = new_context_->GetURLRequestContext()->
        http_transaction_factory()->GetSession()->http_auth_cache();
    new_cache->UpdateAllFrom(*auth_context_->GetURLRequestContext()->
        http_transaction_factory()->GetSession()->http_auth_cache());
  }

 private:
  net::URLRequestContextGetter* auth_context_;
  net::URLRequestContextGetter* new_context_;

  DISALLOW_COPY_AND_ASSIGN(TransferDefaultAuthCacheOnIOThreadTask);
};

// Verifies OAuth1 access token by performing OAuthLogin. Fetches user cookies
// on successful OAuth authentication.
class OAuthLoginVerifier : public base::SupportsWeakPtr<OAuthLoginVerifier>,
                           public GaiaOAuthConsumer,
                           public GaiaAuthConsumer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnOAuthVerificationSucceeded(const std::string& user_name,
                                              const std::string& sid,
                                              const std::string& lsid,
                                              const std::string& auth) {}
    virtual void OnOAuthVerificationFailed(const std::string& user_name) {}
    virtual void OnUserCookiesFetchSucceeded(const std::string& user_name) {}
    virtual void OnUserCookiesFetchFailed(const std::string& user_name) {}
  };

  OAuthLoginVerifier(OAuthLoginVerifier::Delegate* delegate,
                     Profile* user_profile,
                     const std::string& oauth1_token,
                     const std::string& oauth1_secret,
                     const std::string& username)
      : delegate_(delegate),
        oauth_fetcher_(this,
            user_profile->GetOffTheRecordProfile()->GetRequestContext(),
            user_profile->GetOffTheRecordProfile(),
            kServiceScopeChromeOS),
        gaia_fetcher_(this,
            std::string(GaiaConstants::kChromeOSSource),
            user_profile->GetRequestContext()),
        oauth1_token_(oauth1_token),
        oauth1_secret_(oauth1_secret),
        username_(username),
        user_profile_(user_profile),
        verification_count_(0),
        step_(VERIFICATION_STEP_UNVERIFIED) {
  }
  virtual ~OAuthLoginVerifier() {}

  bool is_done() {
    return step_ == VERIFICATION_STEP_FAILED ||
           step_ == VERIFICATION_STEP_COOKIES_FETCHED;
  }

  void StartOAuthVerification() {
    if (oauth1_token_.empty() || oauth1_secret_.empty()) {
      // Empty OAuth1 access token or secret probably means that we are
      // dealing with a legacy ChromeOS account. This should be treated as
      // invalid/expired token.
      OnOAuthLoginFailure(GoogleServiceAuthError(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    } else {
      oauth_fetcher_.StartOAuthLogin(GaiaConstants::kChromeOSSource,
                                     GaiaConstants::kPicasaService,
                                     oauth1_token_,
                                     oauth1_secret_);
    }
  }

  void ContinueVerification() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // Check if we have finished with this one already.
    if (is_done())
      return;

    if (user_profile_ != ProfileManager::GetDefaultProfile())
      return;

    // Check if we currently trying to fetch something.
    if (oauth_fetcher_.HasPendingFetch() || gaia_fetcher_.HasPendingFetch())
      return;

    if (CrosLibrary::Get()->EnsureLoaded()) {
      // Delay the verification if the network is not connected or on a captive
      // portal.
      const Network* network =
          CrosLibrary::Get()->GetNetworkLibrary()->active_network();
      if (!network || !network->connected() || network->restricted_pool()) {
        BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
            base::Bind(&OAuthLoginVerifier::ContinueVerification, AsWeakPtr()),
            kOAuthVerificationRestartDelay);
        return;
      }
    }

    verification_count_++;
    if (step_ == VERIFICATION_STEP_UNVERIFIED) {
      DVLOG(1) << "Retrying to verify OAuth1 access tokens.";
      StartOAuthVerification();
    } else {
      DVLOG(1) << "Retrying to fetch user cookies.";
      StartCookiesRetreival();
    }
  }

 private:
  typedef enum {
    VERIFICATION_STEP_UNVERIFIED,
    VERIFICATION_STEP_OAUTH_VERIFIED,
    VERIFICATION_STEP_COOKIES_FETCHED,
    VERIFICATION_STEP_FAILED,
  } VerificationStep;

  // Kicks off GAIA session cookie retreival process.
  void StartCookiesRetreival() {
    DCHECK(!sid_.empty());
    DCHECK(!lsid_.empty());
    gaia_fetcher_.StartIssueAuthToken(sid_, lsid_, GaiaConstants::kGaiaService);
  }

  // Decides how to proceed on GAIA response and other errors. It can schedule
  // to rerun the verification process if detects transient network or service
  // errors.
  bool RetryOnError(const GoogleServiceAuthError& error) {
    // If we can't connect to GAIA due to network or service related reasons,
    // we should attempt OAuth token verification again.
    if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED ||
        error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
      if (verification_count_ < kMaxOAuthTokenVerificationAttemptCount) {
        BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
            base::Bind(&OAuthLoginVerifier::ContinueVerification, AsWeakPtr()),
            kOAuthVerificationRestartDelay);
        return true;
      }
    }
    step_ = VERIFICATION_STEP_FAILED;
    return false;
  }

  // GaiaOAuthConsumer implementation:
  virtual void OnOAuthLoginSuccess(const std::string& sid,
                                   const std::string& lsid,
                                   const std::string& auth) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    step_ = VERIFICATION_STEP_OAUTH_VERIFIED;
    verification_count_ = 0;
    sid_ = sid;
    lsid_ = lsid;
    delegate_->OnOAuthVerificationSucceeded(username_, sid, lsid, auth);
    StartCookiesRetreival();
  }

  virtual void OnOAuthLoginFailure(
      const GoogleServiceAuthError& error) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    LOG(WARNING) << "Failed to verify OAuth1 access tokens,"
                 << " error.state=" << error.state();
    if (!RetryOnError(error))
      delegate_->OnOAuthVerificationFailed(username_);
  }

  void OnCookueFetchFailed(const GoogleServiceAuthError& error) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!RetryOnError(error))
      delegate_->OnUserCookiesFetchFailed(username_);
  }

  // GaiaAuthConsumer overrides.
  virtual void OnIssueAuthTokenSuccess(const std::string& service,
                                       const std::string& auth_token) OVERRIDE {
    gaia_fetcher_.StartMergeSession(auth_token);
  }

  virtual void OnIssueAuthTokenFailure(const std::string& service,
      const GoogleServiceAuthError& error) OVERRIDE {
    DVLOG(1) << "Failed IssueAuthToken request,"
             << " error.state=" << error.state();
    OnCookueFetchFailed(error);
  }

  virtual void OnMergeSessionSuccess(const std::string& data) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DVLOG(1) << "MergeSession successful.";
    step_ = VERIFICATION_STEP_COOKIES_FETCHED;
    delegate_->OnUserCookiesFetchSucceeded(username_);
  }

  virtual void OnMergeSessionFailure(
      const GoogleServiceAuthError& error) OVERRIDE {
    DVLOG(1) << "Failed MergeSession request,"
             << " error.state=" << error.state();
    OnCookueFetchFailed(error);
  }

  OAuthLoginVerifier::Delegate* delegate_;
  GaiaOAuthFetcher oauth_fetcher_;
  GaiaAuthFetcher gaia_fetcher_;
  std::string oauth1_token_;
  std::string oauth1_secret_;
  std::string sid_;
  std::string lsid_;
  std::string username_;
  Profile* user_profile_;
  int verification_count_;
  VerificationStep step_;

  DISALLOW_COPY_AND_ASSIGN(OAuthLoginVerifier);
};

// Fetches the oauth token for the device management service. Since Profile
// creation might be blocking on a user policy fetch, this fetcher must always
// send a (possibly empty) token to the BrowserPolicyConnector, which will then
// let the policy subsystem proceed and resume Profile creation.
// Sending the token even when no Profile is pending is also OK.
class PolicyOAuthFetcher : public GaiaOAuthConsumer {
 public:
  // Fetches the device management service's oauth token using |oauth1_token|
  // and |oauth1_secret| as access tokens.
  PolicyOAuthFetcher(Profile* profile,
                     const std::string& oauth1_token,
                     const std::string& oauth1_secret)
      : oauth_fetcher_(this,
                       profile->GetRequestContext(),
                       profile,
                       kServiceScopeChromeOSDeviceManagement),
        oauth1_token_(oauth1_token),
        oauth1_secret_(oauth1_secret) {
  }

  // Fetches the device management service's oauth token, after also retrieving
  // the access tokens.
  explicit PolicyOAuthFetcher(Profile* profile)
      : oauth_fetcher_(this,
                       profile->GetRequestContext(),
                       profile,
                       kServiceScopeChromeOSDeviceManagement) {
  }

  virtual ~PolicyOAuthFetcher() {}

  void Start() {
    oauth_fetcher_.SetAutoFetchLimit(
        GaiaOAuthFetcher::OAUTH2_SERVICE_ACCESS_TOKEN);

    if (oauth1_token_.empty()) {
      oauth_fetcher_.StartGetOAuthTokenRequest();
    } else {
      oauth_fetcher_.StartOAuthWrapBridge(
          oauth1_token_, oauth1_secret_, GaiaConstants::kGaiaOAuthDuration,
          std::string(kServiceScopeChromeOSDeviceManagement));
    }
  }

  const std::string& oauth1_token() const { return oauth1_token_; }
  const std::string& oauth1_secret() const { return oauth1_secret_; }
  bool failed() const {
    return !oauth_fetcher_.HasPendingFetch() && policy_token_.empty();
  }

 private:
  virtual void OnGetOAuthTokenSuccess(const std::string& oauth_token) OVERRIDE {
    VLOG(1) << "Got OAuth request token";
  }

  virtual void OnGetOAuthTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE {
    LOG(WARNING) << "Failed to get OAuth request token, error: "
                 << error.state();
    SetPolicyToken("");
  }

  virtual void OnOAuthGetAccessTokenSuccess(
      const std::string& token,
      const std::string& secret) OVERRIDE {
    VLOG(1) << "Got OAuth access token";
    oauth1_token_ = token;
    oauth1_secret_ = secret;
  }

  virtual void OnOAuthGetAccessTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE {
    LOG(WARNING) << "Failed to get OAuth access token, error: "
                 << error.state();
    SetPolicyToken("");
  }

  virtual void OnOAuthWrapBridgeSuccess(
      const std::string& service_name,
      const std::string& token,
      const std::string& expires_in) OVERRIDE {
    VLOG(1) << "Got OAuth access token for " << service_name;
    SetPolicyToken(token);
  }

  virtual void OnOAuthWrapBridgeFailure(
      const std::string& service_name,
      const GoogleServiceAuthError& error) OVERRIDE {
    LOG(WARNING) << "Failed to get OAuth access token for " << service_name
                 << ", error: " << error.state();
    SetPolicyToken("");
  }

  void SetPolicyToken(const std::string& token) {
    policy_token_ = token;
    g_browser_process->browser_policy_connector()->RegisterForUserPolicy(token);
  }

  GaiaOAuthFetcher oauth_fetcher_;
  std::string oauth1_token_;
  std::string oauth1_secret_;
  std::string policy_token_;

  DISALLOW_COPY_AND_ASSIGN(PolicyOAuthFetcher);
};

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
  void RestartJob() {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      DBusThreadManager::Get()->GetSessionManagerClient()->RestartJob(
          pid_, command_line_);
    } else {
      // This function can be called on FILE thread. See PostTask in the
      // constructor.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &JobRestartRequest::RestartJob));
      MessageLoop::current()->AssertIdle();
    }
  }

  int pid_;
  std::string command_line_;
  PrefService* local_state_;
  base::OneShotTimer<JobRestartRequest> timer_;
};

class LoginUtilsImpl : public LoginUtils,
                       public ProfileManagerObserver,
                       public GaiaOAuthConsumer,
                       public OAuthLoginVerifier::Delegate,
                       public net::NetworkChangeNotifier::OnlineStateObserver {
 public:
  LoginUtilsImpl()
      : background_view_(NULL),
        pending_requests_(false),
        using_oauth_(false),
        has_cookies_(false),
        delegate_(NULL),
        job_restart_request_(NULL) {
    net::NetworkChangeNotifier::AddOnlineStateObserver(this);
  }

  virtual ~LoginUtilsImpl() {
    net::NetworkChangeNotifier::RemoveOnlineStateObserver(this);
  }

  // LoginUtils implementation:
  virtual void PrepareProfile(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
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
  virtual void RestoreAuthenticationSession(const std::string& user_name,
                                            Profile* profile) OVERRIDE;
  virtual void StartTokenServices(Profile* user_profile) OVERRIDE;
  virtual void StartSync(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials) OVERRIDE;
  virtual void SetBackgroundView(
      chromeos::BackgroundView* background_view) OVERRIDE;
  virtual chromeos::BackgroundView* GetBackgroundView() OVERRIDE;
  virtual void TransferDefaultCookies(Profile* default_profile,
                                      Profile* new_profile) OVERRIDE;
  virtual void TransferDefaultAuthCache(Profile* default_profile,
                                        Profile* new_profile) OVERRIDE;

  // ProfileManagerObserver implementation:
  virtual void OnProfileCreated(Profile* profile, Status status) OVERRIDE;

  // GaiaOAuthConsumer overrides.
  virtual void OnGetOAuthTokenSuccess(const std::string& oauth_token) OVERRIDE;
  virtual void OnGetOAuthTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnOAuthGetAccessTokenSuccess(const std::string& token,
                                            const std::string& secret) OVERRIDE;
  virtual void OnOAuthGetAccessTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  // OAuthLoginVerifier::Delegate overrides.
  virtual void OnOAuthVerificationSucceeded(const std::string& user_name,
                                            const std::string& sid,
                                            const std::string& lsid,
                                            const std::string& auth) OVERRIDE;
  virtual void OnOAuthVerificationFailed(const std::string& user_name) OVERRIDE;

  // net::NetworkChangeNotifier::OnlineStateObserver overrides.
  virtual void OnOnlineStateChanged(bool online) OVERRIDE;

  // Given the authenticated credentials from the cookie jar, try to exchange
  // fetch OAuth request, v1 and v2 tokens.
  void FetchOAuth1AccessToken(Profile* auth_profile);

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

  // The current background view.
  chromeos::BackgroundView* background_view_;

  std::string username_;
  std::string password_;
  GaiaAuthConsumer::ClientLoginResult credentials_;
  bool pending_requests_;
  bool using_oauth_;
  bool has_cookies_;
  // Has to be scoped_refptr, see comment for CreateAuthenticator(...).
  scoped_refptr<Authenticator> authenticator_;
  scoped_ptr<GaiaOAuthFetcher> oauth_fetcher_;
  scoped_ptr<PolicyOAuthFetcher> policy_oauth_fetcher_;
  scoped_ptr<OAuthLoginVerifier> oauth_login_verifier_;

  // Delegate to be fired when the profile will be prepared.
  LoginUtils::Delegate* delegate_;

  // Used to restart Chrome to switch to the guest mode.
  JobRestartRequest* job_restart_request_;

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

void LoginUtilsImpl::PrepareProfile(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& credentials,
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
  UserManager::Get()->UserLoggedIn(username);
  btl->AddLoginTimeMarker("UserLoggedIn-End", false);

  // Switch log file as soon as possible.
  logging::RedirectChromeLogging(*(CommandLine::ForCurrentProcess()));

  username_ = username;
  password_ = password;

  credentials_ = credentials;
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
          policy::CloudPolicyDataStore::USER_AFFILIATION_MANAGED);

  // Initialize user policy before the profile is created so the profile
  // initialization code sees the cached policy settings.
  connector->InitializeUserPolicy(username, wait_for_policy_fetch);

  if (wait_for_policy_fetch) {
    // Profile creation will block until user policy is fetched, which
    // requires the DeviceManagement token. Try to fetch it now.
    VLOG(1) << "Profile creation requires policy token, fetching now";
    policy_oauth_fetcher_.reset(
        new PolicyOAuthFetcher(authenticator_->authentication_profile()));
    policy_oauth_fetcher_->Start();
  }

  // The default profile will have been changed because the ProfileManager
  // will process the notification that the UserManager sends out.
  ProfileManager::CreateDefaultProfileAsync(this);
}

void LoginUtilsImpl::DelegateDeleted(LoginUtils::Delegate* delegate) {
  if (delegate_ == delegate)
    delegate_ = NULL;
}

void LoginUtilsImpl::OnProfileCreated(Profile* user_profile, Status status) {
  CHECK(user_profile);
  switch (status) {
    case STATUS_INITIALIZED:
      break;
    case STATUS_CREATED:
      if (UserManager::Get()->current_user_is_new())
        SetFirstLoginPrefs(user_profile->GetPrefs());
      RespectLocalePreference(user_profile);
      return;
    case STATUS_FAIL:
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
      // Use off-the-record profile that was used for this step. It should
      // already contain all needed cookies that will let us skip GAIA's user
      // authentication UI.
      //
      // TODO(rickcam) We should use an isolated App here.
      FetchOAuth1AccessToken(authenticator_->authentication_profile());
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

  // TODO(altimofeev): This pointer should probably never be NULL, but it looks
  // like LoginUtilsImpl::OnProfileCreated() may be getting called before
  // LoginUtilsImpl::PrepareProfile() has set |delegate_| when Chrome is killed
  // during shutdown in tests -- see http://crosbug.com/18269.  Replace this
  // 'if' statement with a CHECK(delegate_) once the underlying issue is
  // resolved.
  if (delegate_)
    delegate_->OnProfilePrepared(user_profile);

  // TODO(altimofeev): Need to sanitize memory used to store password.
  credentials_ = GaiaAuthConsumer::ClientLoginResult();
}

void LoginUtilsImpl::FetchOAuth1AccessToken(Profile* auth_profile) {
  oauth_fetcher_.reset(new GaiaOAuthFetcher(this,
                                            auth_profile->GetRequestContext(),
                                            auth_profile,
                                            kServiceScopeChromeOS));
  // Let's first get the Oauth request token and OAuth1 token+secret.
  // Once we get that, we will kick off individual requests for OAuth2 tokens
  // for all our services.
  oauth_fetcher_->SetAutoFetchLimit(GaiaOAuthFetcher::OAUTH1_ALL_ACCESS_TOKEN);
  oauth_fetcher_->StartGetOAuthTokenRequest();
}

void LoginUtilsImpl::StartTokenServices(Profile* user_profile) {
  std::string oauth1_token;
  std::string oauth1_secret;
  if (!ReadOAuth1AccessToken(user_profile, &oauth1_token, &oauth1_secret))
    return;

  FetchSecondaryTokens(user_profile->GetOffTheRecordProfile(), oauth1_token,
                       oauth1_secret);
}

void LoginUtilsImpl::StartSync(
    Profile* user_profile,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  TokenService* token_service = user_profile->GetTokenService();
  static bool initialized = false;
  if (!initialized) {
    initialized = true;

    // Set the CrOS user by getting this constructor run with the
    // user's email on first retrieval.
    user_profile->GetProfileSyncService(username_)->SetPassphrase(
        password_, false);
    username_ = "";
    password_ = "";

    token_service->Initialize(GaiaConstants::kChromeOSSource, user_profile);
    token_service->LoadTokensFromDB();
  }
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
      switches::kEnableLogging,
      switches::kDisableAcceleratedPlugins,
      switches::kUseGL,
      switches::kUserDataDir,
      switches::kScrollPixels,
      switches::kEnableGView,
      switches::kEnableSensors,
      switches::kNoFirstRun,
      switches::kLoginProfile,
      switches::kCompressSystemFeedback,
      switches::kDisableSeccompSandbox,
      switches::kPpapiFlashInProcess,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
      switches::kViewsDesktop,
      switches::kTouchDevices,
#if defined(TOUCH_UI)
      // The virtual keyboard extension for TOUCH_UI (chrome://keyboard) highly
      // relies on experimental APIs.
      switches::kEnableExperimentalExtensionApis,
#endif
  };
  command_line->CopySwitchesFrom(base_command_line,
                                 kForwardSwitches,
                                 arraysize(kForwardSwitches));
  command_line->AppendSwitch(switches::kGuestSession);
  command_line->AppendSwitch(switches::kIncognito);
  command_line->AppendSwitchASCII(switches::kLoggingLevel,
                                 kGuestModeLoggingLevel);

  command_line->AppendSwitchASCII(switches::kLoginUser, kGuestUserName);

  if (start_url.is_valid())
    command_line->AppendArg(start_url.spec());

  // Override the value of the homepage that is set in first run mode.
  // TODO(altimofeev): extend action of the |kNoFirstRun| to cover this case.
  command_line->AppendSwitchASCII(
      switches::kHomePage,
      GURL(chrome::kChromeUINewTabURL).spec());

  std::string cmd_line_str = command_line->GetCommandLineString();
  // Special workaround for the arguments that should be quoted.
  // Copying switches won't be needed when Guest mode won't need restart
  // http://crosbug.com/6924
  if (base_command_line.HasSwitch(switches::kRegisterPepperPlugins)) {
    cmd_line_str += base::StringPrintf(
        kSwitchFormatString,
        switches::kRegisterPepperPlugins,
        base_command_line.GetSwitchValueNative(
            switches::kRegisterPepperPlugins).c_str());
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
      locale, manager->current_input_method(), &input_method_ids);
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
  prefs->ScheduleSavePersistentPrefs();
}

scoped_refptr<Authenticator> LoginUtilsImpl::CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  // Screen locker needs new Authenticator instance each time.
  if (ScreenLocker::default_screen_locker())
    authenticator_ = NULL;

  // In case of non-WebUI login new instance of Authenticator is supposed
  // to be created on each call.
  // TODO(nkostylev): Clean up after WebUI login migration is complete.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kWebUILogin))
    authenticator_ = NULL;

  if (authenticator_ == NULL)
    authenticator_ = new ParallelAuthenticator(consumer);
  return authenticator_;
}

// We use a special class for this so that it can be safely leaked if we
// never connect. At shutdown the order is not well defined, and it's possible
// for the infrastructure needed to unregister might be unstable and crash.
class WarmingObserver : public NetworkLibrary::NetworkManagerObserver {
 public:
  WarmingObserver() {
    NetworkLibrary *netlib = CrosLibrary::Get()->GetNetworkLibrary();
    netlib->AddNetworkManagerObserver(this);
  }

  virtual ~WarmingObserver() {}

  // If we're now connected, prewarm the auth url.
  virtual void OnNetworkManagerChanged(NetworkLibrary* netlib) {
    if (netlib->Connected()) {
      const int kConnectionsNeeded = 1;
      chrome_browser_net::PreconnectOnUIThread(
          GURL(GaiaUrls::GetInstance()->client_login_url()),
          chrome_browser_net::UrlInfo::EARLY_LOAD_MOTIVATED,
          kConnectionsNeeded);
      netlib->RemoveNetworkManagerObserver(this);
      delete this;
    }
  }
};

void LoginUtilsImpl::PrewarmAuthentication() {
  NetworkLibrary *network = CrosLibrary::Get()->GetNetworkLibrary();
  if (network->Connected()) {
    const int kConnectionsNeeded = 1;
    chrome_browser_net::PreconnectOnUIThread(
        GURL(GaiaUrls::GetInstance()->client_login_url()),
        chrome_browser_net::UrlInfo::EARLY_LOAD_MOTIVATED,
        kConnectionsNeeded);
  } else {
    new WarmingObserver();
  }
}

void LoginUtilsImpl::RestoreAuthenticationSession(const std::string& username,
                                                  Profile* user_profile) {
  username_ = username;
  KickStartAuthentication(user_profile);
}

void LoginUtilsImpl::KickStartAuthentication(Profile* user_profile) {
  if (!authenticator_.get())
    CreateAuthenticator(NULL);
  std::string oauth1_token;
  std::string oauth1_secret;
  if (ReadOAuth1AccessToken(user_profile, &oauth1_token, &oauth1_secret))
    VerifyOAuth1AccessToken(user_profile, oauth1_token, oauth1_secret);

  authenticator_ = NULL;
}

void LoginUtilsImpl::SetBackgroundView(BackgroundView* background_view) {
  background_view_ = background_view;
}

BackgroundView* LoginUtilsImpl::GetBackgroundView() {
  return background_view_;
}

void LoginUtilsImpl::TransferDefaultCookies(Profile* default_profile,
                                            Profile* profile) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          new TransferDefaultCookiesOnIOThreadTask(
                              default_profile->GetRequestContext(),
                              profile->GetRequestContext()));
}

void LoginUtilsImpl::TransferDefaultAuthCache(Profile* default_profile,
                                              Profile* profile) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          new TransferDefaultAuthCacheOnIOThreadTask(
                              default_profile->GetRequestContext(),
                              profile->GetRequestContext()));
}

void LoginUtilsImpl::OnGetOAuthTokenSuccess(const std::string& oauth_token) {
  VLOG(1) << "Got OAuth request token!";
}

void LoginUtilsImpl::OnGetOAuthTokenFailure(
    const GoogleServiceAuthError& error) {
  // TODO(zelidrag): Pop up sync setup UI here?
  LOG(WARNING) << "Failed fetching OAuth request token, error: "
               << error.state();
}

void LoginUtilsImpl::OnOAuthGetAccessTokenSuccess(const std::string& token,
                                                  const std::string& secret) {
  VLOG(1) << "Got OAuth v1 token!";
  Profile* user_profile = ProfileManager::GetDefaultProfile();
  StoreOAuth1AccessToken(user_profile, token, secret);

  // Verify OAuth1 token by doing OAuthLogin and fetching credentials.
  VerifyOAuth1AccessToken(user_profile, token, secret);
}

void LoginUtilsImpl::OnOAuthGetAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  // TODO(zelidrag): Pop up sync setup UI here?
  LOG(WARNING) << "Failed fetching OAuth request token, error: "
               << error.state();
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
  if (UserManager::Get()->GetUserOAuthStatus(username_) !=
      User::OAUTH_TOKEN_STATUS_VALID) {
    return false;
  }

  PrefService* pref_service = user_profile->GetPrefs();
  std::string encoded_token = pref_service->GetString(prefs::kOAuth1Token);
  std::string encoded_secret = pref_service->GetString(prefs::kOAuth1Secret);
  if (!encoded_token.length() || !encoded_secret.length())
    return false;

  DCHECK(authenticator_.get());
  std::string decoded_token = authenticator_->DecryptToken(encoded_token);
  std::string decoded_secret = authenticator_->DecryptToken(encoded_secret);
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
  PrefService* pref_service = user_profile->GetPrefs();
  pref_service->SetString(prefs::kOAuth1Token,
                          authenticator_->EncryptToken(token));
  pref_service->SetString(prefs::kOAuth1Secret,
                          authenticator_->EncryptToken(secret));

  // ...then record the presence of valid OAuth token for this account in local
  // state as well.
  UserManager::Get()->SaveUserOAuthStatus(username_,
                                          User::OAUTH_TOKEN_STATUS_VALID);
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
  oauth_login_verifier_.reset(new OAuthLoginVerifier(this,
                                                     user_profile,
                                                     token,
                                                     secret,
                                                     username_));
  oauth_login_verifier_->StartOAuthVerification();
}


void LoginUtilsImpl::FetchPolicyToken(Profile* offrecord_profile,
                                      const std::string& token,
                                      const std::string& secret) {
  // Fetch dm service token now, if it hasn't been fetched yet.
  if (!policy_oauth_fetcher_.get() || policy_oauth_fetcher_->failed()) {
    // Trigger oauth token fetch for user policy.
    policy_oauth_fetcher_.reset(new PolicyOAuthFetcher(offrecord_profile,
                                                       token,
                                                       secret));
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

void LoginUtilsImpl::OnOAuthVerificationSucceeded(
    const std::string& user_name, const std::string& sid,
    const std::string& lsid, const std::string& auth) {
  // Kick off sync engine.
  GaiaAuthConsumer::ClientLoginResult credentials(sid, lsid, auth,
                                                  std::string());
  StartSync(ProfileManager::GetDefaultProfile(), credentials);
}


void LoginUtilsImpl::OnOnlineStateChanged(bool online) {
  // If we come online for the first time after successful offline login,
  // we need to kick of OAuth token verification process again.
  if (online && UserManager::Get()->user_is_logged_in() &&
      oauth_login_verifier_.get() &&
      !oauth_login_verifier_->is_done()) {
    oauth_login_verifier_->ContinueVerification();
  }
}

LoginUtils* LoginUtils::Get() {
  return LoginUtilsWrapper::GetInstance()->get();
}

void LoginUtils::Set(LoginUtils* mock) {
  LoginUtilsWrapper::GetInstance()->reset(mock);
}

void LoginUtils::DoBrowserLaunch(Profile* profile,
                                 LoginDisplayHost* login_host) {
  if (browser_shutdown::IsTryingToQuit())
    return;

  BootTimesLoader::Get()->AddLoginTimeMarker("BrowserLaunched", false);

  VLOG(1) << "Launching browser...";
  BrowserInit browser_init;
  int return_code;
  BrowserInit::IsFirstRun first_run = FirstRun::IsChromeFirstRun() ?
      BrowserInit::IS_FIRST_RUN: BrowserInit::IS_NOT_FIRST_RUN;
  browser_init.LaunchBrowser(*CommandLine::ForCurrentProcess(),
                             profile,
                             FilePath(),
                             BrowserInit::IS_PROCESS_STARTUP,
                             first_run,
                             &return_code);

  // Mark login host for deletion after browser starts.  This
  // guarantees that the message loop will be referenced by the
  // browser before it is dereferenced by the login host.
  if (login_host) {
    login_host->OnSessionStart();
    login_host = NULL;
  }
}

}  // namespace chromeos
