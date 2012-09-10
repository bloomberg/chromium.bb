// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_signin_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/user_cloud_policy_manager.h"
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
    Profile* profile,
    UserCloudPolicyManager* manager)
    : profile_(profile),
      manager_(manager) {

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

UserPolicySigninService::~UserPolicySigninService() {
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
        // TokenService now has a refresh token, so reconfigure the
        // UserCloudPolicyManager to initiate a DMToken fetch if needed.
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
  if (!manager_)
    return;  // Can be null in unit tests.

  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile_);
  if (signin_manager->GetAuthenticatedUsername().empty()) {
    manager_->ShutdownAndRemovePolicy();
  } else {
    if (!manager_->cloud_policy_service()) {
      // Make sure we've initialized the DeviceManagementService. It's OK to
      // call this multiple times so we do it every time we initialize the
      // UserCloudPolicyManager.
      g_browser_process->browser_policy_connector()->
          ScheduleServiceInitialization(
              kPolicyServiceInitializationDelayMilliseconds);
      // Initialize the UserCloudPolicyManager if it isn't already initialized.
      policy::DeviceManagementService* service = g_browser_process->
          browser_policy_connector()->device_management_service();
      manager_->Initialize(g_browser_process->local_state(),
                           service,
                           policy::USER_AFFILIATION_NONE);
      DCHECK(manager_->cloud_policy_service());
    }

    // Register the CloudPolicyService if the cloud policy store is complete.
    // The code below is somewhat racy in that if the store is not initialized
    // by the time we get here, we won't register the client. In practice, we
    // handle the case where there's no policy file because checking for the
    // file always completes before the token DB is loaded (since they use the
    // same thread for their operations).
    // TODO(atwilson): If there's a problem loading the stored policy, we could
    // be left with no policy, so we should move this code to
    // UserCloudPolicyManager and have it initiate a DMToken fetch only once
    // the policy load is complete (http://crbug.com/143187).
    if (!manager_->IsClientRegistered() &&
        manager_->cloud_policy_store()->is_initialized()) {
      RegisterCloudPolicyService();
    }
  }
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
  manager_->CancelWaitForPolicyFetch();
}

void UserPolicySigninService::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  // Pass along the new access token to the CloudPolicyClient.
  DVLOG(1) << "Fetched new scoped OAuth token:" << access_token;
  manager_->RegisterClient(access_token);
  oauth2_access_token_fetcher_.reset();
}

}  // namespace policy
