// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_service.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
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

}  // namespace

namespace chromeos {

DeviceSettingsService::Observer::~Observer() {}

void DeviceSettingsService::Observer::OwnershipStatusChanged() {}

void DeviceSettingsService::Observer::DeviceSettingsUpdated() {}

void DeviceSettingsService::Observer::OnDeviceSettingsServiceShutdown() {}

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
    : load_retries_left_(kMaxLoadRetries), weak_factory_(this) {}

DeviceSettingsService::~DeviceSettingsService() {
  DCHECK(pending_operations_.empty());
  for (auto& observer : observers_)
    observer.OnDeviceSettingsServiceShutdown();
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
  pending_operations_.clear();

  if (session_manager_client_)
    session_manager_client_->RemoveObserver(this);
  session_manager_client_ = NULL;
  owner_key_util_ = NULL;
}

void DeviceSettingsService::SetDeviceMode(policy::DeviceMode device_mode) {
  // Device mode can only change once.
  DCHECK_EQ(policy::DEVICE_MODE_PENDING, device_mode_);
  device_mode_ = device_mode;
  if (GetOwnershipStatus() != OWNERSHIP_UNKNOWN) {
    RunPendingOwnershipStatusCallbacks();
  }
}

scoped_refptr<PublicKey> DeviceSettingsService::GetPublicKey() {
  return public_key_;
}

void DeviceSettingsService::Load() {
  EnqueueLoad(false);
}

void DeviceSettingsService::LoadImmediately() {
  bool request_key_load = true;
  bool cloud_validations = true;
  if (device_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD) {
    request_key_load = false;
    cloud_validations = false;
  }
  std::unique_ptr<SessionManagerOperation> operation(new LoadSettingsOperation(
      request_key_load, cloud_validations, true /*force_immediate_load*/,
      base::Bind(&DeviceSettingsService::HandleCompletedOperation,
                 weak_factory_.GetWeakPtr(), base::Closure())));
  operation->Start(session_manager_client_, owner_key_util_, public_key_);
}

void DeviceSettingsService::Store(
    std::unique_ptr<em::PolicyFetchResponse> policy,
    const base::Closure& callback) {
  // On Active Directory managed devices policy is written only by authpolicyd.
  CHECK(device_mode_ != policy::DEVICE_MODE_ENTERPRISE_AD);
  Enqueue(linked_ptr<SessionManagerOperation>(new StoreSettingsOperation(
      base::Bind(&DeviceSettingsService::HandleCompletedAsyncOperation,
                 weak_factory_.GetWeakPtr(), callback),
      std::move(policy))));
}

DeviceSettingsService::OwnershipStatus
    DeviceSettingsService::GetOwnershipStatus() {
  if (public_key_.get())
    return public_key_->is_loaded() ? OWNERSHIP_TAKEN : OWNERSHIP_NONE;
  if (device_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD)
    return OWNERSHIP_TAKEN;
  return OWNERSHIP_UNKNOWN;
}

void DeviceSettingsService::GetOwnershipStatusAsync(
    const OwnershipStatusCallback& callback) {
  if (GetOwnershipStatus() != OWNERSHIP_UNKNOWN) {
    // Report status immediately.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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

  // Reset the flag since consumer ownership should be established now.
  will_establish_consumer_ownership_ = false;

  EnsureReload(true);
}

const std::string& DeviceSettingsService::GetUsername() const {
  return username_;
}

ownership::OwnerSettingsService*
DeviceSettingsService::GetOwnerSettingsService() const {
  return owner_settings_service_.get();
}

void DeviceSettingsService::MarkWillEstablishConsumerOwnership() {
  will_establish_consumer_ownership_ = true;
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

  if (GetOwnershipStatus() == OWNERSHIP_TAKEN ||
      !will_establish_consumer_ownership_) {
    EnsureReload(true);
  }
}

void DeviceSettingsService::PropertyChangeComplete(bool success) {
  if (!success) {
    LOG(ERROR) << "Policy update failed.";
    return;
  }

  if (GetOwnershipStatus() == OWNERSHIP_TAKEN ||
      !will_establish_consumer_ownership_) {
    EnsureReload(false);
  }
}

void DeviceSettingsService::Enqueue(
    const linked_ptr<SessionManagerOperation>& operation) {
  pending_operations_.push_back(operation);
  if (pending_operations_.front().get() == operation.get())
    StartNextOperation();
}

void DeviceSettingsService::EnqueueLoad(bool request_key_load) {
  bool cloud_validations = true;
  if (device_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD) {
    request_key_load = false;
    cloud_validations = false;
  }
  linked_ptr<SessionManagerOperation> operation(new LoadSettingsOperation(
      request_key_load, cloud_validations, false /*force_immediate_load*/,
      base::Bind(&DeviceSettingsService::HandleCompletedAsyncOperation,
                 weak_factory_.GetWeakPtr(), base::Closure())));
  Enqueue(operation);
}

void DeviceSettingsService::EnsureReload(bool request_key_load) {
  if (!pending_operations_.empty())
    pending_operations_.front()->RestartLoad(request_key_load);
  else
    EnqueueLoad(request_key_load);
}

void DeviceSettingsService::StartNextOperation() {
  if (!pending_operations_.empty() && session_manager_client_ &&
      owner_key_util_.get()) {
    pending_operations_.front()->Start(
        session_manager_client_, owner_key_util_, public_key_);
  }
}

void DeviceSettingsService::HandleCompletedAsyncOperation(
    const base::Closure& callback,
    SessionManagerOperation* operation,
    Status status) {
  DCHECK_EQ(operation, pending_operations_.front().get());
  HandleCompletedOperation(callback, operation, status);
  // Only remove the pending operation here, so new operations triggered by
  // any of the callbacks above are queued up properly.
  pending_operations_.pop_front();

  StartNextOperation();
}

void DeviceSettingsService::HandleCompletedOperation(
    const base::Closure& callback,
    SessionManagerOperation* operation,
    Status status) {
  store_status_ = status;
  if (status == STORE_SUCCESS) {
    policy_data_ = std::move(operation->policy_data());
    device_settings_ = std::move(operation->device_settings());
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

  public_key_ = scoped_refptr<PublicKey>(operation->public_key());
  if (GetOwnershipStatus() != previous_ownership_status_) {
    previous_ownership_status_ = GetOwnershipStatus();
    NotifyOwnershipStatusChanged();
  }
  NotifyDeviceSettingsUpdated();
  RunPendingOwnershipStatusCallbacks();

  // The completion callback happens after the notification so clients can
  // filter self-triggered updates.
  if (!callback.is_null())
    callback.Run();
}

void DeviceSettingsService::HandleError(Status status,
                                        const base::Closure& callback) {
  store_status_ = status;
  LOG(ERROR) << "Session manager operation failed: " << status;
  NotifyDeviceSettingsUpdated();

  // The completion callback happens after the notification so clients can
  // filter self-triggered updates.
  if (!callback.is_null())
    callback.Run();
}

void DeviceSettingsService::NotifyOwnershipStatusChanged() const {
  for (auto& observer : observers_) {
    observer.OwnershipStatusChanged();
  }
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED,
      content::Source<DeviceSettingsService>(this),
      content::NotificationService::NoDetails());
}

void DeviceSettingsService::NotifyDeviceSettingsUpdated() const {
  for (auto& observer : observers_)
    observer.DeviceSettingsUpdated();
}

void DeviceSettingsService::RunPendingOwnershipStatusCallbacks() {
  std::vector<OwnershipStatusCallback> callbacks;
  callbacks.swap(pending_ownership_status_callbacks_);
  for (const auto& callback : callbacks) {
    callback.Run(GetOwnershipStatus());
  }
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
