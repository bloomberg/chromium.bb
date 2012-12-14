// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_signin_service.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/user_cloud_policy_manager.h"
#include "chrome/browser/policy/user_cloud_policy_manager_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

namespace {
// Various OAuth service scopes required to do CloudPolicyClient registration.
const char kServiceScopeChromeOSDeviceManagement[] =
    "https://www.googleapis.com/auth/chromeosdevicemanagement";
const char kServiceScopeGetUserInfo[] =
    "https://www.googleapis.com/auth/userinfo.email";

// The key under which the hosted-domain value is stored in the UserInfo
// response.
const char kGetHostedDomainKey[] = "hd";

// How long to delay before starting device policy network requests. Set to 10
// seconds to alleviate contention during initial startup.
const int64 kPolicyServiceInitializationDelayMilliseconds = 10000;
}  // namespace

namespace policy {

UserPolicySigninService::UserPolicySigninService(
    Profile* profile)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      profile_(profile),
      pending_fetch_(false) {

  if (!profile_->GetPrefs()->GetBoolean(prefs::kLoadCloudPolicyOnSignin))
    return;

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

  // Make sure we've initialized the DeviceManagementService. It's OK to
  // call this multiple times so we do it every time we create an instance of
  // this class (every time a new profile is loaded).
  g_browser_process->browser_policy_connector()->
      ScheduleServiceInitialization(
          kPolicyServiceInitializationDelayMilliseconds);
}

UserPolicySigninService::~UserPolicySigninService() {}

void UserPolicySigninService::FetchPolicyForSignedInUser(
    const std::string& oauth2_access_token,
    const PolicyFetchCallback& callback) {
  if (!ShouldLoadPolicyForSignedInUser()) {
    callback.Run(false);
    return;
  }

  // If the DeviceManagementService is not yet initialized, start it up now.
  g_browser_process->browser_policy_connector()->
      ScheduleServiceInitialization(0);

  // The user has just signed in, so the UserCloudPolicyManager should not yet
  // be initialized, and the client should not be registered because there
  // should be no cached policy. This routine will proactively ask the client
  // to register itself without waiting for the CloudPolicyService to finish
  // initialization.
  DCHECK(!GetManager()->core()->service());
  InitializeUserCloudPolicyManager();
  DCHECK(!GetManager()->IsClientRegistered());

  DCHECK(!pending_fetch_);
  pending_fetch_ = true;
  pending_fetch_callback_ = callback;

  // Register the client using this access token.
  RegisterCloudPolicyService(oauth2_access_token);
}

void UserPolicySigninService::StopObserving() {
  UserCloudPolicyManager* manager = GetManager();
  if (manager) {
    if (manager->core()->service())
      manager->core()->service()->RemoveObserver(this);
    if (manager->core()->client())
      manager->core()->client()->RemoveObserver(this);
  }
}

void UserPolicySigninService::StartObserving() {
  UserCloudPolicyManager* manager = GetManager();
  // Manager should be fully initialized by now.
  DCHECK(manager);
  DCHECK(manager->core()->service());
  DCHECK(manager->core()->client());
  manager->core()->service()->AddObserver(this);
  manager->core()->client()->AddObserver(this);
}

void UserPolicySigninService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
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
      if (signin_manager->GetAuthenticatedUsername().empty())
        ShutdownUserCloudPolicyManager();
      else
        InitializeUserCloudPolicyManager();
      break;
    }
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      const TokenService::TokenAvailableDetails& token_details =
          *(content::Details<const TokenService::TokenAvailableDetails>(
              details).ptr());
      if (token_details.service() ==
          GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
        // TokenService now has a refresh token (implying that the user is
        // signed in) so initialize the UserCloudPolicyManager.
        InitializeUserCloudPolicyManager();
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

bool UserPolicySigninService::ShouldLoadPolicyForSignedInUser() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kLoadCloudPolicyOnSignin))
    return false; // Cloud policy is disabled.

  const std::string& username = SigninManagerFactory::GetForProfile(profile_)->
      GetAuthenticatedUsername();

  if (username.empty())
    return false; // Not signed in.

  return !BrowserPolicyConnector::IsNonEnterpriseUser(username);
}

void UserPolicySigninService::InitializeUserCloudPolicyManager() {
  if (!ShouldLoadPolicyForSignedInUser()) {
    VLOG(1) << "Policy load not enabled for user: " <<
        SigninManagerFactory::GetForProfile(profile_)->
            GetAuthenticatedUsername();
    return;
  }

  UserCloudPolicyManager* manager = GetManager();
  DCHECK(!SigninManagerFactory::GetForProfile(profile_)->
         GetAuthenticatedUsername().empty());
  if (!manager->core()->service()) {
    // If there is no cached DMToken then we can detect this below (or when
    // the OnInitializationCompleted() callback is invoked).
    BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    manager->Connect(g_browser_process->local_state(),
                     connector->device_management_service());
    DCHECK(manager->core()->service());
    StartObserving();
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
  oauth2_access_token_fetcher_.reset();
  user_info_fetcher_.reset();
  oauth_access_token_.clear();

  StopObserving();
  NotifyPendingFetchCallback(false);

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
}

void UserPolicySigninService::RegisterCloudPolicyService(
    std::string login_token) {
  DCHECK(!GetManager()->IsClientRegistered());
  DVLOG(1) << "Fetching new DM Token";
  // Do nothing if already starting the registration process.
  if (oauth2_access_token_fetcher_.get() ||
      user_info_fetcher_.get() ||
      !oauth_access_token_.empty()) {
    return;
  }

  // Start fetching an OAuth2 access token for the device management and
  // userinfo services.
  oauth2_access_token_fetcher_.reset(
      new OAuth2AccessTokenFetcher(this, profile_->GetRequestContext()));
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

void UserPolicySigninService::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  DLOG(WARNING) << "Could not fetch access token for "
                << kServiceScopeChromeOSDeviceManagement;
  oauth2_access_token_fetcher_.reset();
  // If there was a pending fetch request, let them know the fetch failed.
  NotifyPendingFetchCallback(false);
}

void UserPolicySigninService::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  oauth_access_token_ = access_token;
  DVLOG(1) << "Fetched new scoped OAuth token:" << oauth_access_token_;
  oauth2_access_token_fetcher_.reset();
  // Now we've gotten our access token - contact GAIA to see if this is a
  // hosted domain.
  user_info_fetcher_.reset(new UserInfoFetcher(this,
                                               profile_->GetRequestContext()));
  user_info_fetcher_->Start(oauth_access_token_);
}

void UserPolicySigninService::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  user_info_fetcher_.reset();
  NotifyPendingFetchCallback(false);
}

void UserPolicySigninService::OnGetUserInfoSuccess(
    const DictionaryValue* data) {
  user_info_fetcher_.reset();
  if (!data->HasKey(kGetHostedDomainKey)) {
    VLOG(1) << "User not from a hosted domain - skipping registration";
    NotifyPendingFetchCallback(false);
    return;
  }
  VLOG(1) << "Registering CloudPolicyClient for user from hosted domain";
  // The user is from a hosted domain, so it's OK to register the
  // CloudPolicyClient and make requests from DMServer.
  GetManager()->RegisterClient(oauth_access_token_);
}

void UserPolicySigninService::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DCHECK_EQ(GetManager()->core()->client(), client);
  if (pending_fetch_) {
    UserCloudPolicyManager* manager = GetManager();
    if (manager->IsClientRegistered()) {
      // Request a policy fetch.
      manager->core()->service()->RefreshPolicy(
          base::Bind(
              &UserPolicySigninService::NotifyPendingFetchCallback,
              weak_factory_.GetWeakPtr()));
    } else {
      // Shouldn't be possible for the client to get unregistered.
      NOTREACHED() << "Client unregistered while waiting for policy fetch";
    }
  }
}

void UserPolicySigninService::NotifyPendingFetchCallback(bool success) {
  if (pending_fetch_) {
    pending_fetch_ = false;
    pending_fetch_callback_.Run(success);
  }
}

void UserPolicySigninService::OnPolicyFetched(CloudPolicyClient* client) {
  // Do nothing when policy is fetched - if the policy fetch is successful,
  // NotifyPendingFetchCallback will be invoked.
}

void UserPolicySigninService::OnClientError(CloudPolicyClient* client) {
  NotifyPendingFetchCallback(false);
}

void UserPolicySigninService::Shutdown() {
  StopObserving();
}

UserCloudPolicyManager* UserPolicySigninService::GetManager() {
  return UserCloudPolicyManagerFactory::GetForProfile(profile_);
}

}  // namespace policy
