// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_signin_service.h"

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
// TODO(atwilson): Move this once we add OAuth token support to TokenService.
const char kServiceScopeChromeOSDeviceManagement[] =
    "https://www.googleapis.com/auth/chromeosdevicemanagement";

// How long to delay before starting device policy network requests. Set to a
// few seconds to alleviate contention during initial startup.
const int64 kPolicyServiceInitializationDelayMilliseconds = 2000;
}  // namespace

namespace policy {

UserPolicySigninService::UserPolicySigninService(
    Profile* profile)
    : profile_(profile) {

  // Initialize/shutdown the UserCloudPolicyManager when the user signs in or
  // out.
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(profile));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(
                     TokenServiceFactory::GetForProfile(profile)));

  // The Profile is not yet fully initialized when this object is created,
  // so wait until the initialization has finished to initialize the
  // UserCloudPolicyManager as otherwise various crashes ensue from services
  // trying to access the partially-initialized Profile.
  // TODO(atwilson): Remove this once ProfileImpl::DoFinalInit() goes away and
  // the profile is fully initialized before ProfileKeyedServices are created.
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_ADDED,
                 content::Source<Profile>(profile));
}

UserPolicySigninService::~UserPolicySigninService() {}

void UserPolicySigninService::StopObserving() {
  UserCloudPolicyManager* manager = GetManager();
  if (manager && manager->cloud_policy_service())
    manager->cloud_policy_service()->RemoveObserver(this);
}

void UserPolicySigninService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_ADDED:
      // Profile is initialized so it's safe to initialize the
      // UserCloudPolicyManager now.
      ConfigureUserCloudPolicyManager();
      break;
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT:
      ConfigureUserCloudPolicyManager();
      break;
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      const TokenService::TokenAvailableDetails& token_details =
          *(content::Details<const TokenService::TokenAvailableDetails>(
              details).ptr());
      if (token_details.service() ==
          GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
        // TokenService now has a refresh token, so initialize the
        // UserCloudPolicyManager.
        ConfigureUserCloudPolicyManager();
      }
      break;
    }
    default:
      NOTREACHED();
  }
}


void UserPolicySigninService::ConfigureUserCloudPolicyManager() {
  // Don't do anything unless cloud policy is enabled.
  if (!profile_->GetPrefs()->GetBoolean(prefs::kLoadCloudPolicyOnSignin))
    return;

  // Either startup or shutdown the UserCloudPolicyManager depending on whether
  // the user is signed in or not.
  UserCloudPolicyManager* manager = GetManager();
  if (!manager)
    return;  // Can be null in unit tests.

  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile_);
  if (signin_manager->GetAuthenticatedUsername().empty()) {
    // User has signed out - remove existing policy.
    StopObserving();
    manager->ShutdownAndRemovePolicy();
  } else {
    // Initialize the UserCloudPolicyManager if it isn't already initialized.
    if (!manager->cloud_policy_service()) {
      // Make sure we've initialized the DeviceManagementService. It's OK to
      // call this multiple times so we do it every time we initialize the
      // UserCloudPolicyManager.
      g_browser_process->browser_policy_connector()->
          ScheduleServiceInitialization(
              kPolicyServiceInitializationDelayMilliseconds);
      // If there is no cached DMToken then we can detect this below (or when
      // the OnInitializationCompleted() callback is invoked.
      policy::DeviceManagementService* service = g_browser_process->
          browser_policy_connector()->device_management_service();
      manager->Initialize(g_browser_process->local_state(), service);
      DCHECK(manager->cloud_policy_service());
      manager->cloud_policy_service()->AddObserver(this);
    }

    // If the CloudPolicyService is initialized, but the CloudPolicyClient still
    // needs to be registered, kick off registration.
    if (manager->cloud_policy_service()->IsInitializationComplete() &&
        !manager->IsClientRegistered()) {
      RegisterCloudPolicyService();
    }
  }
}

void UserPolicySigninService::OnInitializationCompleted(
    CloudPolicyService* service) {
  UserCloudPolicyManager* manager = GetManager();
  DCHECK_EQ(service, manager->cloud_policy_service());
  DCHECK(service->IsInitializationComplete());
  // The service is now initialized - if the client is not yet registered, then
  // it means that there is no cached policy and so we need to initiate a new
  // client registration.
  DVLOG_IF(1, manager->IsClientRegistered())
      << "Client already registered - not fetching DMToken";
  if (!manager->IsClientRegistered())
    RegisterCloudPolicyService();
}

void UserPolicySigninService::RegisterCloudPolicyService() {
  DVLOG(1) << "Fetching new DM Token";
  // TODO(atwilson): Move the code to mint the devicemanagement token into
  // TokenService.
  std::string token = TokenServiceFactory::GetForProfile(profile_)->
                          GetOAuth2LoginRefreshToken();
  if (token.empty()) {
    DLOG(WARNING) << "No OAuth Refresh Token - delaying policy download";
    return;
  }

  // Do nothing if already fetching an access token.
  if (oauth2_access_token_fetcher_.get())
    return;

  // Start fetching an OAuth2 access token for the device management service and
  // hand it off to the CloudPolicyClient when done.
  oauth2_access_token_fetcher_.reset(
      new OAuth2AccessTokenFetcher(this, profile_->GetRequestContext()));
  std::vector<std::string> scopes(1, kServiceScopeChromeOSDeviceManagement);
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  oauth2_access_token_fetcher_->Start(
      gaia_urls->oauth2_chrome_client_id(),
      gaia_urls->oauth2_chrome_client_secret(),
      token,
      scopes);
}

void UserPolicySigninService::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  DLOG(WARNING) << "Could not fetch access token for "
                << kServiceScopeChromeOSDeviceManagement;
  oauth2_access_token_fetcher_.reset();
}

void UserPolicySigninService::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  UserCloudPolicyManager* manager = GetManager();
  // Pass along the new access token to the CloudPolicyClient.
  DVLOG(1) << "Fetched new scoped OAuth token:" << access_token;
  manager->RegisterClient(access_token);
  oauth2_access_token_fetcher_.reset();
}

void UserPolicySigninService::Shutdown() {
  StopObserving();
}

UserCloudPolicyManager* UserPolicySigninService::GetManager() {
  return UserCloudPolicyManagerFactory::GetForProfile(profile_);
}

}  // namespace policy
