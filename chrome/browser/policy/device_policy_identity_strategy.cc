// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_identity_strategy.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/guid.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace policy {

// Responsible for querying device ownership on the FILE thread.
class DevicePolicyIdentityStrategy::OwnershipChecker
    : public base::RefCountedThreadSafe<
          DevicePolicyIdentityStrategy::OwnershipChecker> {
 public:
  explicit OwnershipChecker(
      const base::WeakPtr<DevicePolicyIdentityStrategy>& strategy)
      : strategy_(strategy) {}

  // Initiates a query on the file thread to check if the currently logged in
  // user is the owner.
  void CheckCurrentUserIsOwner();

 private:
  void CheckOnFileThread();
  void CallbackOnUIThread(bool current_user_is_owner);

 private:
  friend class base::RefCountedThreadSafe<OwnershipChecker>;

  ~OwnershipChecker() {}

  // The object to be called back with the result.
  base::WeakPtr<DevicePolicyIdentityStrategy> strategy_;

  DISALLOW_COPY_AND_ASSIGN(OwnershipChecker);
};

void DevicePolicyIdentityStrategy::OwnershipChecker::CheckCurrentUserIsOwner() {
  if (!strategy_.get())
    return;
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevicePolicyIdentityStrategy::OwnershipChecker::CheckOnFileThread));
}

void DevicePolicyIdentityStrategy::OwnershipChecker::CheckOnFileThread() {
  bool current_user_is_owner =
      chromeos::OwnershipService::GetSharedInstance()->CurrentUserIsOwner();
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevicePolicyIdentityStrategy::OwnershipChecker::CallbackOnUIThread,
          current_user_is_owner));
}

void DevicePolicyIdentityStrategy::OwnershipChecker::CallbackOnUIThread(
    bool current_user_is_owner) {
  if (strategy_.get()) {
    strategy_->OnOwnershipInformationAvailable(current_user_is_owner);
    strategy_.reset();
  }
}

DevicePolicyIdentityStrategy::DevicePolicyIdentityStrategy()
    : current_user_is_owner_(false),
      ownership_checker_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  registrar_.Add(this,
                 NotificationType::TOKEN_AVAILABLE,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::LOGIN_USER_CHANGED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED,
                 NotificationService::AllSources());

  // TODO(mnissler): Figure out how to read the machine id.
  machine_id_ = "dummy-cros-machine-ID";
}

DevicePolicyIdentityStrategy::~DevicePolicyIdentityStrategy() {
}

void DevicePolicyIdentityStrategy::OnOwnershipInformationAvailable(
    bool current_user_is_owner) {
  current_user_is_owner_ = current_user_is_owner;
  CheckAndTriggerFetch();
}

void DevicePolicyIdentityStrategy::CheckOwnershipAndTriggerFetch() {
  // TODO(gfeher): Avoid firing a new query if the answer is already known.

  // Cancel any pending queries.
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Set to false until we know that the current user is the owner.
  current_user_is_owner_ = false;
  // Issue a new query.
  ownership_checker_ = new OwnershipChecker(weak_ptr_factory_.GetWeakPtr());
  // The following will call back to CheckTriggerFetch().
  ownership_checker_->CheckCurrentUserIsOwner();
}

std::string DevicePolicyIdentityStrategy::GetDeviceToken() {
  return device_token_;
}

std::string DevicePolicyIdentityStrategy::GetDeviceID() {
  return device_id_;
}

std::string DevicePolicyIdentityStrategy::GetMachineID() {
  return machine_id_;
}

em::DeviceRegisterRequest_Type
DevicePolicyIdentityStrategy::GetPolicyRegisterType() {
  return em::DeviceRegisterRequest::DEVICE;
}

std::string DevicePolicyIdentityStrategy::GetPolicyType() {
  return kChromeDevicePolicyType;
}

bool DevicePolicyIdentityStrategy::GetCredentials(std::string* username,
                                                  std::string* auth_token) {
  // Need to know the machine id.
  if (machine_id_.empty())
    return false;

  // Only fetch credentials (and, subsequently, token/policy) when the owner
  // is logged in.
  if (!current_user_is_owner_)
    return false;

  // We need to know about the profile of the logged in user.
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  if (!profile) {
    NOTREACHED() << "Current user profile inaccessible";
    return false;
  }

  *username = chromeos::UserManager::Get()->logged_in_user().email();
  *auth_token = profile->GetTokenService()->GetTokenForService(
      GaiaConstants::kDeviceManagementService);

  return !username->empty() && !auth_token->empty();
}

void DevicePolicyIdentityStrategy::OnDeviceTokenAvailable(
    const std::string& token) {
  DCHECK(!machine_id_.empty());

  device_token_ = token;
  NotifyDeviceTokenChanged();
}

void DevicePolicyIdentityStrategy::CheckAndTriggerFetch() {
  std::string username;
  std::string auth_token;
  if (GetCredentials(&username, &auth_token)) {
    device_id_ = guid::GenerateGUID();
    NotifyAuthChanged();
  }
}

void DevicePolicyIdentityStrategy::Observe(NotificationType type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  if (type == NotificationType::TOKEN_AVAILABLE) {
    const TokenService::TokenAvailableDetails* token_details =
        Details<const TokenService::TokenAvailableDetails>(details).ptr();
    if (token_details->service() == GaiaConstants::kDeviceManagementService)
      CheckOwnershipAndTriggerFetch();
  } else if (type == NotificationType::LOGIN_USER_CHANGED) {
    CheckOwnershipAndTriggerFetch();
  } else if (type == NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED) {
    CheckOwnershipAndTriggerFetch();
  } else {
    NOTREACHED();
  }
}

}  // namespace policy
