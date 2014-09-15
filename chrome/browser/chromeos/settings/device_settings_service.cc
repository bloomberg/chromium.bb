// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/session_manager_operation.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "crypto/rsa_private_key.h"

namespace em = enterprise_management;

using ownership::OwnerKeyUtil;
using ownership::PublicKey;

namespace {

// Delay between load retries when there was a validation error.
// NOTE: This code is here to mitigate clock loss on some devices where policy
// loads will fail with a validation error caused by RTC clock being reset when
// the battery is drained.
int kLoadRetryDelayMs = 1000 * 5;
// Maximal number of retries before we give up. Calculated to allow for 10 min
// of retry time.
int kMaxLoadRetries = (1000 * 60 * 10) / kLoadRetryDelayMs;

// Assembles PolicyData based on |settings|, |policy_data| and
// |user_id|.
scoped_ptr<em::PolicyData> AssemblePolicy(
    const std::string& user_id,
    const em::PolicyData* policy_data,
    const em::ChromeDeviceSettingsProto* settings) {
  scoped_ptr<em::PolicyData> policy(new em::PolicyData());
  if (policy_data) {
    // Preserve management settings.
    if (policy_data->has_management_mode())
      policy->set_management_mode(policy_data->management_mode());
    if (policy_data->has_request_token())
      policy->set_request_token(policy_data->request_token());
    if (policy_data->has_device_id())
      policy->set_device_id(policy_data->device_id());
  } else {
    // If there's no previous policy data, this is the first time the device
    // setting is set. We set the management mode to NOT_MANAGED initially.
    policy->set_management_mode(em::PolicyData::NOT_MANAGED);
  }
  policy->set_policy_type(policy::dm_protocol::kChromeDevicePolicyType);
  policy->set_timestamp(
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds());
  policy->set_username(user_id);
  if (!settings->SerializeToString(policy->mutable_policy_value()))
    return scoped_ptr<em::PolicyData>();

  return policy.Pass();
}

// Returns true if it is okay to transfer from the current mode to the new
// mode. This function should be called in SetManagementMode().
bool CheckManagementModeTransition(em::PolicyData::ManagementMode current_mode,
                                   em::PolicyData::ManagementMode new_mode) {
  // Mode is not changed.
  if (current_mode == new_mode)
    return true;

  switch (current_mode) {
    case em::PolicyData::NOT_MANAGED:
      // For consumer management enrollment.
      return new_mode == em::PolicyData::CONSUMER_MANAGED;

    case em::PolicyData::ENTERPRISE_MANAGED:
      // Management mode cannot be set when it is currently ENTERPRISE_MANAGED.
      return false;

    case em::PolicyData::CONSUMER_MANAGED:
      // For consumer management unenrollment.
      return new_mode == em::PolicyData::NOT_MANAGED;
  }

  NOTREACHED();
  return false;
}

}  // namespace

namespace chromeos {

DeviceSettingsService::Observer::~Observer() {}

static DeviceSettingsService* g_device_settings_service = NULL;

// static
void DeviceSettingsService::Initialize() {
  CHECK(!g_device_settings_service);
  g_device_settings_service = new DeviceSettingsService();
}

// static
bool DeviceSettingsService::IsInitialized() {
  return g_device_settings_service;
}

// static
void DeviceSettingsService::Shutdown() {
  DCHECK(g_device_settings_service);
  delete g_device_settings_service;
  g_device_settings_service = NULL;
}

// static
DeviceSettingsService* DeviceSettingsService::Get() {
  CHECK(g_device_settings_service);
  return g_device_settings_service;
}

DeviceSettingsService::DeviceSettingsService()
    : session_manager_client_(NULL),
      store_status_(STORE_SUCCESS),
      load_retries_left_(kMaxLoadRetries),
      weak_factory_(this) {
}

DeviceSettingsService::~DeviceSettingsService() {
  DCHECK(pending_operations_.empty());
}

void DeviceSettingsService::SetSessionManager(
    SessionManagerClient* session_manager_client,
    scoped_refptr<OwnerKeyUtil> owner_key_util) {
  DCHECK(session_manager_client);
  DCHECK(owner_key_util.get());
  DCHECK(!session_manager_client_);
  DCHECK(!owner_key_util_.get());

  session_manager_client_ = session_manager_client;
  owner_key_util_ = owner_key_util;

  session_manager_client_->AddObserver(this);

  StartNextOperation();
}

void DeviceSettingsService::UnsetSessionManager() {
  STLDeleteContainerPointers(pending_operations_.begin(),
                             pending_operations_.end());
  pending_operations_.clear();

  if (session_manager_client_)
    session_manager_client_->RemoveObserver(this);
  session_manager_client_ = NULL;
  owner_key_util_ = NULL;
}

scoped_refptr<PublicKey> DeviceSettingsService::GetPublicKey() {
  return public_key_;
}

void DeviceSettingsService::Load() {
  EnqueueLoad(false);
}

void DeviceSettingsService::SignAndStore(
    scoped_ptr<em::ChromeDeviceSettingsProto> new_settings,
    const base::Closure& callback) {
  if (!owner_settings_service_) {
    HandleError(STORE_KEY_UNAVAILABLE, callback);
    return;
  }
  scoped_ptr<em::PolicyData> policy =
      AssemblePolicy(GetUsername(), policy_data(), new_settings.get());
  if (!policy) {
    HandleError(STORE_POLICY_ERROR, callback);
    return;
  }

  owner_settings_service_->SignAndStorePolicyAsync(policy.Pass(), callback);
}

void DeviceSettingsService::SetManagementSettings(
    em::PolicyData::ManagementMode management_mode,
    const std::string& request_token,
    const std::string& device_id,
    const base::Closure& callback) {
  if (!owner_settings_service_) {
    HandleError(STORE_KEY_UNAVAILABLE, callback);
    return;
  }

  em::PolicyData::ManagementMode current_mode = em::PolicyData::NOT_MANAGED;
  if (policy_data() && policy_data()->has_management_mode())
    current_mode = policy_data()->management_mode();

  if (!CheckManagementModeTransition(current_mode, management_mode)) {
    LOG(ERROR) << "Invalid management mode transition: current mode = "
               << current_mode << ", new mode = " << management_mode;
    HandleError(DeviceSettingsService::STORE_POLICY_ERROR, callback);
    return;
  }

  scoped_ptr<em::PolicyData> policy =
      AssemblePolicy(GetUsername(), policy_data(), device_settings());
  if (!policy) {
    HandleError(DeviceSettingsService::STORE_POLICY_ERROR, callback);
    return;
  }

  policy->set_management_mode(management_mode);
  policy->set_request_token(request_token);
  policy->set_device_id(device_id);

  owner_settings_service_->SignAndStorePolicyAsync(policy.Pass(), callback);
}

void DeviceSettingsService::Store(scoped_ptr<em::PolicyFetchResponse> policy,
                                  const base::Closure& callback) {
  Enqueue(
      new StoreSettingsOperation(
          base::Bind(&DeviceSettingsService::HandleCompletedOperation,
                     weak_factory_.GetWeakPtr(),
                     callback),
          policy.Pass()));
}

DeviceSettingsService::OwnershipStatus
    DeviceSettingsService::GetOwnershipStatus() {
  if (public_key_.get())
    return public_key_->is_loaded() ? OWNERSHIP_TAKEN : OWNERSHIP_NONE;
  return OWNERSHIP_UNKNOWN;
}

void DeviceSettingsService::GetOwnershipStatusAsync(
    const OwnershipStatusCallback& callback) {
  if (public_key_.get()) {
    // If there is a key, report status immediately.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GetOwnershipStatus()));
  } else {
    // If the key hasn't been loaded yet, enqueue the callback to be fired when
    // the next SessionManagerOperation completes. If no operation is pending,
    // start a load operation to fetch the key and report the result.
    pending_ownership_status_callbacks_.push_back(callback);
    if (pending_operations_.empty())
      EnqueueLoad(false);
  }
}

bool DeviceSettingsService::HasPrivateOwnerKey() {
  return owner_settings_service_ && owner_settings_service_->IsOwner();
}

void DeviceSettingsService::InitOwner(
    const std::string& username,
    const base::WeakPtr<ownership::OwnerSettingsService>&
        owner_settings_service) {
  // When InitOwner() is called twice with the same |username| it's
  // worth to reload settings since owner key may become available.
  if (!username_.empty() && username_ != username)
    return;
  username_ = username;
  owner_settings_service_ = owner_settings_service;

  EnsureReload(true);
}

const std::string& DeviceSettingsService::GetUsername() const {
  return username_;
}

void DeviceSettingsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceSettingsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceSettingsService::OwnerKeySet(bool success) {
  if (!success) {
    LOG(ERROR) << "Owner key change failed.";
    return;
  }

  public_key_ = NULL;
  EnsureReload(true);
}

void DeviceSettingsService::PropertyChangeComplete(bool success) {
  if (!success) {
    LOG(ERROR) << "Policy update failed.";
    return;
  }

  EnsureReload(false);
}

void DeviceSettingsService::Enqueue(SessionManagerOperation* operation) {
  pending_operations_.push_back(operation);
  if (pending_operations_.front() == operation)
    StartNextOperation();
}

void DeviceSettingsService::EnqueueLoad(bool force_key_load) {
  SessionManagerOperation* operation =
      new LoadSettingsOperation(
          base::Bind(&DeviceSettingsService::HandleCompletedOperation,
                     weak_factory_.GetWeakPtr(),
                     base::Closure()));
  operation->set_force_key_load(force_key_load);
  operation->set_username(username_);
  operation->set_owner_settings_service(owner_settings_service_);
  Enqueue(operation);
}

void DeviceSettingsService::EnsureReload(bool force_key_load) {
  if (!pending_operations_.empty()) {
    pending_operations_.front()->set_username(username_);
    pending_operations_.front()->set_owner_settings_service(
        owner_settings_service_);
    pending_operations_.front()->RestartLoad(force_key_load);
  } else {
    EnqueueLoad(force_key_load);
  }
}

void DeviceSettingsService::StartNextOperation() {
  if (!pending_operations_.empty() &&
      session_manager_client_ &&
      owner_key_util_.get()) {
    pending_operations_.front()->Start(
        session_manager_client_, owner_key_util_, public_key_);
  }
}

void DeviceSettingsService::HandleCompletedOperation(
    const base::Closure& callback,
    SessionManagerOperation* operation,
    Status status) {
  DCHECK_EQ(operation, pending_operations_.front());
  store_status_ = status;

  OwnershipStatus ownership_status = OWNERSHIP_UNKNOWN;
  scoped_refptr<PublicKey> new_key(operation->public_key());
  if (new_key.get()) {
    ownership_status = new_key->is_loaded() ? OWNERSHIP_TAKEN : OWNERSHIP_NONE;
  } else {
    NOTREACHED() << "Failed to determine key status.";
  }

  bool new_owner_key = false;
  if (public_key_.get() != new_key.get()) {
    public_key_ = new_key;
    new_owner_key = true;
  }

  if (status == STORE_SUCCESS) {
    policy_data_ = operation->policy_data().Pass();
    device_settings_ = operation->device_settings().Pass();
    load_retries_left_ = kMaxLoadRetries;
  } else if (status != STORE_KEY_UNAVAILABLE) {
    LOG(ERROR) << "Session manager operation failed: " << status;
    // Validation errors can be temporary if the rtc has gone on holiday for a
    // short while. So we will retry such loads for up to 10 minutes.
    if (status == STORE_TEMP_VALIDATION_ERROR) {
      if (load_retries_left_ > 0) {
        load_retries_left_--;
        LOG(ERROR) << "A re-load has been scheduled due to a validation error.";
        content::BrowserThread::PostDelayedTask(
            content::BrowserThread::UI,
            FROM_HERE,
            base::Bind(&DeviceSettingsService::Load, base::Unretained(this)),
            base::TimeDelta::FromMilliseconds(kLoadRetryDelayMs));
      } else {
        // Once we've given up retrying, the validation error is not temporary
        // anymore.
        store_status_ = STORE_VALIDATION_ERROR;
      }
    }
  }

  if (new_owner_key) {
    FOR_EACH_OBSERVER(Observer, observers_, OwnershipStatusChanged());
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED,
        content::Source<DeviceSettingsService>(this),
        content::NotificationService::NoDetails());
  }

  FOR_EACH_OBSERVER(Observer, observers_, DeviceSettingsUpdated());

  std::vector<OwnershipStatusCallback> callbacks;
  callbacks.swap(pending_ownership_status_callbacks_);
  for (std::vector<OwnershipStatusCallback>::iterator iter(callbacks.begin());
       iter != callbacks.end(); ++iter) {
    iter->Run(ownership_status);
  }

  // The completion callback happens after the notification so clients can
  // filter self-triggered updates.
  if (!callback.is_null())
    callback.Run();

  // Only remove the pending operation here, so new operations triggered by any
  // of the callbacks above are queued up properly.
  pending_operations_.pop_front();
  delete operation;

  StartNextOperation();
}

void DeviceSettingsService::HandleError(Status status,
                                        const base::Closure& callback) {
  store_status_ = status;

  LOG(ERROR) << "Session manager operation failed: " << status;

  FOR_EACH_OBSERVER(Observer, observers_, DeviceSettingsUpdated());

  // The completion callback happens after the notification so clients can
  // filter self-triggered updates.
  if (!callback.is_null())
    callback.Run();
}

void DeviceSettingsService::OnSignAndStoreOperationCompleted(Status status) {
  store_status_ = status;
  FOR_EACH_OBSERVER(Observer, observers_, DeviceSettingsUpdated());
}

ScopedTestDeviceSettingsService::ScopedTestDeviceSettingsService() {
  DeviceSettingsService::Initialize();
}

ScopedTestDeviceSettingsService::~ScopedTestDeviceSettingsService() {
  // Clean pending operations.
  DeviceSettingsService::Get()->UnsetSessionManager();
  DeviceSettingsService::Shutdown();
}

}  // namespace chromeos
