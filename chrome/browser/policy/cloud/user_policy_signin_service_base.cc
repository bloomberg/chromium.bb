// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_base.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/device_management_service.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_source.h"
#include "net/url_request/url_request_context_getter.h"

namespace policy {

UserPolicySigninServiceBase::UserPolicySigninServiceBase(
    Profile* profile,
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    DeviceManagementService* device_management_service)
    : profile_(profile),
      local_state_(local_state),
      request_context_(request_context),
      device_management_service_(device_management_service),
      weak_factory_(this) {
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableCloudPolicyOnSignin))
    return;

  // Initialize/shutdown the UserCloudPolicyManager when the user signs out.
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(profile));

  // Register a listener to be called back once the current profile has finished
  // initializing, so we can startup the UserCloudPolicyManager.
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_ADDED,
                 content::Source<Profile>(profile));
}

UserPolicySigninServiceBase::~UserPolicySigninServiceBase() {}

void UserPolicySigninServiceBase::FetchPolicyForSignedInUser(
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

void UserPolicySigninServiceBase::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // If using a TestingProfile with no SigninManager or UserCloudPolicyManager,
  // skip initialization.
  if (!GetManager() || !GetSigninManager()) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT:
      ShutdownUserCloudPolicyManager();
      break;
    case chrome::NOTIFICATION_PROFILE_ADDED:
      // A new profile has been loaded - if it's signed in, then initialize the
      // UCPM, otherwise shut down the UCPM (which deletes any cached policy
      // data). This must be done here instead of at constructor time because
      // the Profile is not fully initialized when this object is constructed
      // (DoFinalInit() has not yet been called, so ProfileIOData and
      // SSLConfigServiceManager have not been created yet).
      // TODO(atwilson): Switch to using a timer instead, to avoid contention
      // with other services at startup (http://crbug.com/165468).
      InitializeOnProfileReady();
      break;
    default:
      NOTREACHED();
  }
}

void UserPolicySigninServiceBase::OnInitializationCompleted(
    CloudPolicyService* service) {
  // This is meant to be overridden by subclasses. Starting and stopping to
  // observe the CloudPolicyService from this base class avoids the need for
  // more virtuals.
}

void UserPolicySigninServiceBase::OnPolicyFetched(CloudPolicyClient* client) {}

void UserPolicySigninServiceBase::OnRegistrationStateChanged(
    CloudPolicyClient* client) {}

void UserPolicySigninServiceBase::OnClientError(CloudPolicyClient* client) {
  if (client->is_registered()) {
    // If the client is already registered, it means this error must have
    // come from a policy fetch.
    if (client->status() == DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED) {
      // OK, policy fetch failed with MANAGEMENT_NOT_SUPPORTED - this is our
      // trigger to revert to "unmanaged" mode (we will check for management
      // being re-enabled on the next restart and/or login).
      DVLOG(1) << "DMServer returned NOT_SUPPORTED error - removing policy";

      // Can't shutdown now because we're in the middle of a callback from
      // the CloudPolicyClient, so queue up a task to do the shutdown.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(
              &UserPolicySigninServiceBase::ShutdownUserCloudPolicyManager,
              weak_factory_.GetWeakPtr()));
    } else {
      DVLOG(1) << "Error fetching policy: " << client->status();
    }
  }
}

void UserPolicySigninServiceBase::Shutdown() {
  PrepareForUserCloudPolicyManagerShutdown();
}

void UserPolicySigninServiceBase::PrepareForUserCloudPolicyManagerShutdown() {
  UserCloudPolicyManager* manager = GetManager();
  if (manager && manager->core()->client())
    manager->core()->client()->RemoveObserver(this);
  if (manager && manager->core()->service())
    manager->core()->service()->RemoveObserver(this);
}

// static
bool UserPolicySigninServiceBase::ShouldForceLoadPolicy() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceLoadCloudPolicy);
}

scoped_ptr<CloudPolicyClient> UserPolicySigninServiceBase::PrepareToRegister(
    const std::string& username) {
  DCHECK(!username.empty());
  // We should not be called with a client already initialized.
  DCHECK(!GetManager() || !GetManager()->core()->client());

  // If the user should not get policy, just bail out.
  if (!GetManager() || !ShouldLoadPolicyForUser(username)) {
    DVLOG(1) << "Signed in user is not in the whitelist";
    return scoped_ptr<CloudPolicyClient>();
  }

  // If the DeviceManagementService is not yet initialized, start it up now.
  device_management_service_->ScheduleInitialization(0);

  // Create a new CloudPolicyClient for fetching the DMToken.
  return UserCloudPolicyManager::CreateCloudPolicyClient(
      device_management_service_);
}

bool UserPolicySigninServiceBase::ShouldLoadPolicyForUser(
    const std::string& username) {
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableCloudPolicyOnSignin))
    return false;  // Cloud policy is disabled.

  if (username.empty())
    return false;  // Not signed in.

  if (ShouldForceLoadPolicy())
    return true;

  return !BrowserPolicyConnector::IsNonEnterpriseUser(username);
}

void UserPolicySigninServiceBase::InitializeOnProfileReady() {
  std::string username = GetSigninManager()->GetAuthenticatedUsername();
  if (username.empty())
    ShutdownUserCloudPolicyManager();
  else
    InitializeForSignedInUser(username);
}

void UserPolicySigninServiceBase::InitializeForSignedInUser(
    const std::string& username) {
  DCHECK(!username.empty());
  if (!ShouldLoadPolicyForUser(username)) {
    DVLOG(1) << "Policy load not enabled for user: " << username;
    return;
  }

  UserCloudPolicyManager* manager = GetManager();
  // Initialize the UCPM if it is not already initialized.
  if (!manager->core()->service()) {
    // If there is no cached DMToken then we can detect this when the
    // OnInitializationCompleted() callback is invoked and this will
    // initiate a policy fetch.
    InitializeUserCloudPolicyManager(
        UserCloudPolicyManager::CreateCloudPolicyClient(
            device_management_service_).Pass());
  }

  // If the CloudPolicyService is initialized, kick off registration.
  // Otherwise OnInitializationCompleted is invoked as soon as the service
  // finishes its initialization.
  if (manager->core()->service()->IsInitializationComplete())
    OnInitializationCompleted(manager->core()->service());
}

void UserPolicySigninServiceBase::InitializeUserCloudPolicyManager(
    scoped_ptr<CloudPolicyClient> client) {
  UserCloudPolicyManager* manager = GetManager();
  DCHECK(!manager->core()->client());
  manager->Connect(local_state_, request_context_, client.Pass());
  DCHECK(manager->core()->service());

  // Observe the client to detect errors fetching policy.
  manager->core()->client()->AddObserver(this);
  // Observe the service to determine when it's initialized.
  manager->core()->service()->AddObserver(this);
}

void UserPolicySigninServiceBase::ShutdownUserCloudPolicyManager() {
  PrepareForUserCloudPolicyManagerShutdown();
  UserCloudPolicyManager* manager = GetManager();
  if (manager)
    manager->DisconnectAndRemovePolicy();
}

UserCloudPolicyManager* UserPolicySigninServiceBase::GetManager() {
  return UserCloudPolicyManagerFactory::GetForProfile(profile_);
}

SigninManager* UserPolicySigninServiceBase::GetSigninManager() {
  return SigninManagerFactory::GetForProfile(profile_);
}

}  // namespace policy
