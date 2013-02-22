// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_signin_service.h"

#include <vector>

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/user_cloud_policy_manager.h"
#include "chrome/browser/policy/user_cloud_policy_manager_factory.h"
#include "chrome/browser/policy/user_info_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

namespace em = enterprise_management;

namespace {

// Various OAuth service scopes required to do CloudPolicyClient registration.
const char kServiceScopeChromeOSDeviceManagement[] =
    "https://www.googleapis.com/auth/chromeosdevicemanagement";
const char kServiceScopeGetUserInfo[] =
    "https://www.googleapis.com/auth/userinfo.email";

// The key under which the hosted-domain value is stored in the UserInfo
// response.
const char kGetHostedDomainKey[] = "hd";

bool ShouldForceLoadPolicy() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceLoadCloudPolicy);
}

}  // namespace

namespace policy {

// Helper class that registers a CloudPolicyClient and returns the associated
// DMToken to the caller.
class CloudPolicyClientRegistrationHelper
    : public policy::CloudPolicyClient::Observer,
      public OAuth2AccessTokenConsumer,
      public policy::UserInfoFetcher::Delegate {
 public:
  explicit CloudPolicyClientRegistrationHelper(
      net::URLRequestContextGetter* context);

  virtual ~CloudPolicyClientRegistrationHelper();

  // Starts the client registration process. Callback is invoked when the
  // registration is complete.
  void StartRegistration(policy::CloudPolicyClient* client,
                         const std::string& oauth2_login_token,
                         base::Closure callback);

  // OAuth2AccessTokenConsumer implementation.
  virtual void OnGetTokenSuccess(const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // UserInfoFetcher::Delegate implementation:
  virtual void OnGetUserInfoSuccess(const DictionaryValue* response) OVERRIDE;
  virtual void OnGetUserInfoFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  // CloudPolicyClient::Observer implementation.
  virtual void OnPolicyFetched(policy::CloudPolicyClient* client) OVERRIDE {}
  virtual void OnRegistrationStateChanged(
      policy::CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(policy::CloudPolicyClient* client) OVERRIDE;

 private:
  // Invoked when the registration request has been completed.
  void RequestCompleted();

  // Fetcher used while obtaining an OAuth token for client registration.
  scoped_ptr<OAuth2AccessTokenFetcher> oauth2_access_token_fetcher_;

  // Helper class for fetching information from GAIA about the currently
  // signed-in user.
  scoped_ptr<policy::UserInfoFetcher> user_info_fetcher_;

  // Access token used to register the CloudPolicyClient and also access
  // GAIA to get information about the signed in user.
  std::string oauth_access_token_;

  net::URLRequestContextGetter* context_;
  policy::CloudPolicyClient* client_;
  base::Closure callback_;
};

CloudPolicyClientRegistrationHelper::CloudPolicyClientRegistrationHelper(
    net::URLRequestContextGetter* context) : context_(context) {
  DCHECK(context_);
}

CloudPolicyClientRegistrationHelper::~CloudPolicyClientRegistrationHelper() {
  // Clean up any pending observers in case the browser is shutdown while
  // trying to register for policy.
  if (client_)
    client_->RemoveObserver(this);
}

void CloudPolicyClientRegistrationHelper::RequestCompleted() {
  if (client_) {
    client_->RemoveObserver(this);
    // |client_| may be freed by the callback so clear it now.
    client_ = NULL;
    callback_.Run();
  }
}

void CloudPolicyClientRegistrationHelper::StartRegistration(
    policy::CloudPolicyClient* client,
    const std::string& login_token,
    base::Closure callback) {
  DVLOG(1) << "Starting registration process";
  DCHECK(client);
  DCHECK(!client->is_registered());
  client_ = client;
  callback_ = callback;
  client_->AddObserver(this);

  // Start fetching an OAuth2 access token for the device management and
  // userinfo services.
  oauth2_access_token_fetcher_.reset(
      new OAuth2AccessTokenFetcher(this, context_));
  std::vector<std::string> scopes;
  scopes.push_back(kServiceScopeChromeOSDeviceManagement);
  scopes.push_back(kServiceScopeGetUserInfo);
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  oauth2_access_token_fetcher_->Start(
      gaia_urls->oauth2_chrome_client_id(),
      gaia_urls->oauth2_chrome_client_secret(),
      login_token,
      scopes);
}

void CloudPolicyClientRegistrationHelper::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  DLOG(WARNING) << "Could not fetch access token for "
                << kServiceScopeChromeOSDeviceManagement;
  oauth2_access_token_fetcher_.reset();

  // Invoke the callback to let them know the fetch failed.
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  // Cache the access token to be used after the GetUserInfo call.
  oauth_access_token_ = access_token;
  DVLOG(1) << "Fetched new scoped OAuth token:" << oauth_access_token_;
  oauth2_access_token_fetcher_.reset();
  // Now we've gotten our access token - contact GAIA to see if this is a
  // hosted domain.
  user_info_fetcher_.reset(new policy::UserInfoFetcher(this, context_));
  user_info_fetcher_->Start(oauth_access_token_);
}

void CloudPolicyClientRegistrationHelper::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "Failed to fetch user info from GAIA: " << error.state();
  user_info_fetcher_.reset();
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::OnGetUserInfoSuccess(
    const DictionaryValue* data) {
  user_info_fetcher_.reset();
  if (!data->HasKey(kGetHostedDomainKey) && !ShouldForceLoadPolicy()) {
    DVLOG(1) << "User not from a hosted domain - skipping registration";
    RequestCompleted();
    return;
  }
  DVLOG(1) << "Registering CloudPolicyClient for user from hosted domain";
  // The user is from a hosted domain, so it's OK to register the
  // CloudPolicyClient and make requests to DMServer.
  if (client_->is_registered()) {
    // Client should not be registered yet.
    NOTREACHED();
    RequestCompleted();
    return;
  }

  // Kick off registration of the CloudPolicyClient with our newly minted
  // oauth_access_token_.
  client_->Register(em::DeviceRegisterRequest::BROWSER, oauth_access_token_,
                    std::string(), false);
}

void CloudPolicyClientRegistrationHelper::OnRegistrationStateChanged(
    policy::CloudPolicyClient* client) {
  DVLOG(1) << "Client registration succeeded";
  DCHECK_EQ(client, client_);
  DCHECK(client->is_registered());
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::OnClientError(
    policy::CloudPolicyClient* client) {
  DVLOG(1) << "Client registration failed";
  DCHECK_EQ(client, client_);
  RequestCompleted();
}

UserPolicySigninService::UserPolicySigninService(
    Profile* profile)
    : profile_(profile) {
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableCloudPolicyOnSignin) ||
      ProfileManager::IsImportProcess(*cmd_line)) {
    return;
  }

  // Initialize/shutdown the UserCloudPolicyManager when the user signs out.
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(profile));

  // Listen for an OAuth token to become available so we can register a client
  // if for some reason the client is not already registered (for example, if
  // the policy load failed during initial signin).
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(
                     TokenServiceFactory::GetForProfile(profile)));

  // TokenService should not yet have loaded its tokens since this happens in
  // the background after PKS initialization - so this service should always be
  // created before the oauth token is available.
  DCHECK(!TokenServiceFactory::GetForProfile(profile_)->HasOAuthLoginToken());

  // Register a listener to be called back once the current profile has finished
  // initializing, so we can startup the UserCloudPolicyManager.
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_ADDED,
                 content::Source<Profile>(profile));
}

UserPolicySigninService::~UserPolicySigninService() {}

void UserPolicySigninService::RegisterPolicyClient(
    const std::string& username,
    const std::string& oauth2_refresh_token,
    const PolicyRegistrationCallback& callback) {
  DCHECK(!username.empty());
  DCHECK(!oauth2_refresh_token.empty());
  // We should not be called with a client already initialized.
  DCHECK(!GetManager() || !GetManager()->core()->client());

  // If the user should not get policy, just bail out.
  if (!GetManager() || !ShouldLoadPolicyForUser(username)) {
    DVLOG(1) << "Signed in user is not in the whitelist";
    callback.Run(scoped_ptr<CloudPolicyClient>().Pass());
    return;
  }

  // If the DeviceManagementService is not yet initialized, start it up now.
  g_browser_process->browser_policy_connector()->
      ScheduleServiceInitialization(0);

  // Create a new CloudPolicyClient for fetching the DMToken.
  scoped_ptr<CloudPolicyClient> policy_client(
      UserCloudPolicyManager::CreateCloudPolicyClient(
          g_browser_process->browser_policy_connector()->
          device_management_service()));

  registration_helper_.reset(
      new CloudPolicyClientRegistrationHelper(profile_->GetRequestContext()));

  // Fire off the registration process. Callback keeps the CloudPolicyClient
  // alive for the length of the registration process.
  // Grab a pointer to the client before base::Bind() clears the reference in
  // |policy_client|.
  CloudPolicyClient* client = policy_client.get();
  base::Closure registration_callback =
      base::Bind(&UserPolicySigninService::CallPolicyRegistrationCallback,
                 base::Unretained(this), base::Passed(&policy_client),
                 callback);
  registration_helper_->StartRegistration(
      client, oauth2_refresh_token, registration_callback);
}

void UserPolicySigninService::CallPolicyRegistrationCallback(
    scoped_ptr<CloudPolicyClient> client,
    PolicyRegistrationCallback callback) {
  registration_helper_.reset();
  if (!client->is_registered()) {
    // Registration failed, so free the client and pass NULL to the callback.
    client.reset();
  }
  callback.Run(client.Pass());
}

void UserPolicySigninService::FetchPolicyForSignedInUser(
    scoped_ptr<CloudPolicyClient> client,
    const PolicyFetchCallback& callback) {
  DCHECK(client);
  DCHECK(client->is_registered());
  // The user has just signed in, so the UserCloudPolicyManager should not yet
  // be initialized. This routine will initialize the UserCloudPolicyManager
  // with the passed client and will proactively ask the client to fetch
  // policy without waiting for the CloudPolicyService to finish initialization.
  UserCloudPolicyManager* manager = GetManager();
  DCHECK(manager);
  DCHECK(!manager->core()->client());
  InitializeUserCloudPolicyManager(client.Pass());
  DCHECK(manager->IsClientRegistered());

  // Now initiate a policy fetch.
  manager->core()->service()->RefreshPolicy(callback);
}

void UserPolicySigninService::StopObserving() {
  UserCloudPolicyManager* manager = GetManager();
  if (manager && manager->core()->service())
    manager->core()->service()->RemoveObserver(this);
}

void UserPolicySigninService::StartObserving() {
  UserCloudPolicyManager* manager = GetManager();
  // Manager should be fully initialized by now.
  DCHECK(manager);
  DCHECK(manager->core()->service());
  manager->core()->service()->AddObserver(this);
}

void UserPolicySigninService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // If using a TestingProfile with no SigninManager or UserCloudPolicyManager,
  // skip initialization.
  if (!GetManager() || !SigninManagerFactory::GetForProfile(profile_)) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT:
      ShutdownUserCloudPolicyManager();
      break;
    case chrome::NOTIFICATION_PROFILE_ADDED: {
      // A new profile has been loaded - if it's signed in, then initialize the
      // UCPM, otherwise shut down the UCPM (which deletes any cached policy
      // data). This must be done here instead of at constructor time because
      // the Profile is not fully initialized when this object is constructed
      // (DoFinalInit() has not yet been called, so ProfileIOData and
      // SSLConfigServiceManager have not been created yet).
      // TODO(atwilson): Switch to using a timer instead, to avoid contention
      // with other services at startup (http://crbug.com/165468).
      SigninManager* signin_manager =
          SigninManagerFactory::GetForProfile(profile_);
      std::string username = signin_manager->GetAuthenticatedUsername();
      if (username.empty())
        ShutdownUserCloudPolicyManager();
      else
        InitializeForSignedInUser();
      break;
    }
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      const TokenService::TokenAvailableDetails& token_details =
          *(content::Details<const TokenService::TokenAvailableDetails>(
              details).ptr());
      if (token_details.service() ==
          GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
        SigninManager* signin_manager =
            SigninManagerFactory::GetForProfile(profile_);
        std::string username = signin_manager->GetAuthenticatedUsername();
        // Should not have GAIA tokens if the user isn't signed in.
        DCHECK(!username.empty());
        // TokenService now has a refresh token (implying that the user is
        // signed in) so initialize the UserCloudPolicyManager.
        InitializeForSignedInUser();
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

bool UserPolicySigninService::ShouldLoadPolicyForUser(
    const std::string& username) {
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableCloudPolicyOnSignin))
    return false; // Cloud policy is disabled.

  if (username.empty())
    return false; // Not signed in.

  if (ShouldForceLoadPolicy())
    return true;

  return !BrowserPolicyConnector::IsNonEnterpriseUser(username);
}

void UserPolicySigninService::InitializeUserCloudPolicyManager(
    scoped_ptr<CloudPolicyClient> client) {
  UserCloudPolicyManager* manager = GetManager();
  DCHECK(!manager->core()->client());
  // If there is no cached DMToken then we can detect this below (or when
  // the OnInitializationCompleted() callback is invoked).
  manager->Connect(g_browser_process->local_state(), client.Pass());
  DCHECK(manager->core()->service());
  StartObserving();
  ProhibitSignoutIfNeeded();
}

void UserPolicySigninService::InitializeForSignedInUser() {
  SigninManager* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  std::string username = signin_manager->GetAuthenticatedUsername();

  if (!ShouldLoadPolicyForUser(username)) {
    DVLOG(1) << "Policy load not enabled for user: " << username;
    return;
  }
  DCHECK(!username.empty());

  UserCloudPolicyManager* manager = GetManager();
  // Initialize the UCPM if it is not already initialized.
  if (!manager->core()->service()) {
    // If there is no cached DMToken then we can detect this when the
    // OnInitializationCompleted() callback is invoked and this will
    // initiate a policy fetch.
    BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    InitializeUserCloudPolicyManager(
        UserCloudPolicyManager::CreateCloudPolicyClient(
            connector->device_management_service()).Pass());
  }

  // If the CloudPolicyService is initialized, kick off registration. If the
  // TokenService doesn't have an OAuth token yet (e.g. this is during initial
  // signin, or when dynamically loading a signed-in policy) this does nothing
  // until the OAuth token is loaded.
  if (manager->core()->service()->IsInitializationComplete())
    OnInitializationCompleted(manager->core()->service());
}

void UserPolicySigninService::ShutdownUserCloudPolicyManager() {
  // Stop any in-progress token fetch.
  registration_helper_.reset();

  StopObserving();

  UserCloudPolicyManager* manager = GetManager();
  if (manager)  // Can be null in unit tests.
    manager->DisconnectAndRemovePolicy();
}

void UserPolicySigninService::OnInitializationCompleted(
    CloudPolicyService* service) {
  UserCloudPolicyManager* manager = GetManager();
  DCHECK_EQ(service, manager->core()->service());
  DCHECK(service->IsInitializationComplete());
  // The service is now initialized - if the client is not yet registered, then
  // it means that there is no cached policy and so we need to initiate a new
  // client registration.
  DVLOG_IF(1, manager->IsClientRegistered())
      << "Client already registered - not fetching DMToken";
  if (!manager->IsClientRegistered()) {
    std::string token = TokenServiceFactory::GetForProfile(profile_)->
        GetOAuth2LoginRefreshToken();
    if (token.empty()) {
      // No token yet - this class listens for NOTIFICATION_TOKEN_AVAILABLE
      // and will re-attempt registration once the token is available.
      DLOG(WARNING) << "No OAuth Refresh Token - delaying policy download";
      return;
    }
    RegisterCloudPolicyService(token);
  }
  // If client is registered now, prohibit signout.
  ProhibitSignoutIfNeeded();
}

void UserPolicySigninService::RegisterCloudPolicyService(
    std::string login_token) {
  DCHECK(!GetManager()->IsClientRegistered());
  DVLOG(1) << "Fetching new DM Token";
  // Do nothing if already starting the registration process.
  if (registration_helper_)
    return;

  // Start the process of registering the CloudPolicyClient. Once it completes,
  // policy fetch will automatically happen.
  registration_helper_.reset(
      new CloudPolicyClientRegistrationHelper(profile_->GetRequestContext()));
  registration_helper_->StartRegistration(
      GetManager()->core()->client(),
      login_token,
      base::Bind(&UserPolicySigninService::OnRegistrationComplete,
                 base::Unretained(this)));
}

void UserPolicySigninService::OnRegistrationComplete() {
  ProhibitSignoutIfNeeded();
  registration_helper_.reset();
}

void UserPolicySigninService::ProhibitSignoutIfNeeded() {
  if (GetManager()->IsClientRegistered()) {
    DVLOG(1) << "User is registered for policy - prohibiting signout";
    SigninManager* signin_manager =
        SigninManagerFactory::GetForProfile(profile_);
    signin_manager->ProhibitSignout();
  }
}

void UserPolicySigninService::Shutdown() {
  // Stop any pending registration helper activity. We do this here instead of
  // in the destructor because we want to shutdown the registration helper
  // before UserCloudPolicyManager shuts down the CloudPolicyClient.
  registration_helper_.reset();
  StopObserving();
}

UserCloudPolicyManager* UserPolicySigninService::GetManager() {
  return UserCloudPolicyManagerFactory::GetForProfile(profile_);
}

}  // namespace policy
