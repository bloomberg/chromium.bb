// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_policy_provider.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/task.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/device_management_policy_cache.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace policy {

const int64 kPolicyRefreshRateInMilliseconds = 3 * 60 * 60 * 1000;  // 3 hours
const int64 kPolicyRefreshMaxEarlierInMilliseconds = 20 * 60 * 1000;  // 20 mins
// These are the base values for delays before retrying after an error. They
// will be doubled each time they are used.
const int64 kPolicyRefreshErrorDelayInMilliseconds = 3 * 1000;  // 3 seconds
const int64 kDeviceTokenRefreshErrorDelayInMilliseconds = 3 * 1000;

// Ensures that the portion of the policy provider implementation that requires
// the IOThread is deferred until the IOThread is fully initialized. The policy
// provider posts this task on the UI thread during its constructor, thereby
// guaranteeing that the code won't get executed until after the UI and IO
// threads are fully constructed.
class DeviceManagementPolicyProvider::InitializeAfterIOThreadExistsTask
    : public Task {
 public:
  explicit InitializeAfterIOThreadExistsTask(
      base::WeakPtr<DeviceManagementPolicyProvider> provider)
      : provider_(provider) {
  }

  // Task implementation:
  virtual void Run() {
    DeviceManagementPolicyProvider* provider = provider_.get();
    if (provider)
      provider->InitializeAfterIOThreadExists();
  }

 private:
  base::WeakPtr<DeviceManagementPolicyProvider> provider_;
};

class DeviceManagementPolicyProvider::RefreshTask : public Task {
 public:
  explicit RefreshTask(base::WeakPtr<DeviceManagementPolicyProvider> provider)
      : provider_(provider) {}

  // Task implementation:
  virtual void Run() {
    DeviceManagementPolicyProvider* provider = provider_.get();
    if (provider)
      provider->RefreshTaskExecute();
  }

 private:
  base::WeakPtr<DeviceManagementPolicyProvider> provider_;
};

DeviceManagementPolicyProvider::DeviceManagementPolicyProvider(
    const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list,
    DeviceManagementBackend* backend,
    Profile* profile)
    : ConfigurationPolicyProvider(policy_list),
      backend_(backend),
      profile_(profile),
      storage_dir_(GetOrCreateDeviceManagementDir(profile_->GetPath())),
      policy_request_pending_(false),
      refresh_task_pending_(false),
      policy_refresh_rate_ms_(kPolicyRefreshRateInMilliseconds),
      policy_refresh_max_earlier_ms_(kPolicyRefreshMaxEarlierInMilliseconds),
      policy_refresh_error_delay_ms_(kPolicyRefreshErrorDelayInMilliseconds),
      token_fetch_error_delay_ms_(kDeviceTokenRefreshErrorDelayInMilliseconds) {
  Initialize();
}

DeviceManagementPolicyProvider::~DeviceManagementPolicyProvider() {}

bool DeviceManagementPolicyProvider::Provide(
    ConfigurationPolicyStoreInterface* policy_store) {
  scoped_ptr<DictionaryValue> policies(cache_->GetPolicy());
  DecodePolicyValueTree(policies.get(), policy_store);
  return true;
}

void DeviceManagementPolicyProvider::HandlePolicyResponse(
    const em::DevicePolicyResponse& response) {
  if (cache_->SetPolicy(response))
    NotifyStoreOfPolicyChange();
  policy_request_pending_ = false;
  // Reset the error delay since policy fetching succeeded this time.
  policy_refresh_error_delay_ms_ = kPolicyRefreshErrorDelayInMilliseconds;
  ScheduleRefreshTask(GetRefreshTaskDelay());
}

void DeviceManagementPolicyProvider::OnError(
    DeviceManagementBackend::ErrorCode code) {
  policy_request_pending_ = false;
  if (code == DeviceManagementBackend::kErrorServiceDeviceNotFound ||
      code == DeviceManagementBackend::kErrorServiceManagementTokenInvalid) {
    LOG(WARNING) << "The device token was either invalid or unknown to the "
                 << "device manager, re-registering device.";
    token_fetcher_->Restart();
  } else {
    LOG(WARNING) << "Could not provide policy from the device manager (error = "
                 << code << "), will retry in "
                 << (policy_refresh_error_delay_ms_/1000) << " seconds.";
    ScheduleRefreshTask(policy_refresh_error_delay_ms_);
    policy_refresh_error_delay_ms_ *= 2;
    if (policy_refresh_rate_ms_ &&
        policy_refresh_rate_ms_ < policy_refresh_error_delay_ms_) {
      policy_refresh_error_delay_ms_ = policy_refresh_rate_ms_;
    }
  }
}

void DeviceManagementPolicyProvider::OnTokenSuccess() {
  if (policy_request_pending_)
    return;
  SendPolicyRequest();
}

void DeviceManagementPolicyProvider::OnTokenError() {
  LOG(WARNING) << "Could not retrieve device token.";
  ScheduleRefreshTask(token_fetch_error_delay_ms_);
  token_fetch_error_delay_ms_ *= 2;
  if (token_fetch_error_delay_ms_ > policy_refresh_rate_ms_)
    token_fetch_error_delay_ms_ = policy_refresh_rate_ms_;
}

void DeviceManagementPolicyProvider::OnNotManaged() {
  VLOG(1) << "This device is not managed.";
}

void DeviceManagementPolicyProvider::Shutdown() {
  profile_ = NULL;
  if (token_fetcher_)
    token_fetcher_->Shutdown();
}

void DeviceManagementPolicyProvider::Initialize() {
  const FilePath policy_path = storage_dir_.Append(
      FILE_PATH_LITERAL("Policy"));
  cache_.reset(new DeviceManagementPolicyCache(policy_path));
  cache_->LoadPolicyFromFile();

  // Defer initialization that requires the IOThread until after the IOThread
  // has been initialized.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          new InitializeAfterIOThreadExistsTask(AsWeakPtr()));
}

void DeviceManagementPolicyProvider::InitializeAfterIOThreadExists() {
  if (profile_) {
    if (!token_fetcher_) {
      token_fetcher_ = new DeviceTokenFetcher(
          backend_.get(), profile_, GetTokenPath());
    }
    registrar_.Init(token_fetcher_);
    registrar_.AddObserver(this);
    token_fetcher_->StartFetching();
  }
}

void DeviceManagementPolicyProvider::SendPolicyRequest() {
  if (!policy_request_pending_) {
    policy_request_pending_ = true;
    em::DevicePolicyRequest policy_request;
    policy_request.set_policy_scope(kChromePolicyScope);
    em::DevicePolicySettingRequest* setting =
        policy_request.add_setting_request();
    setting->set_key(kChromeDevicePolicySettingKey);
    backend_->ProcessPolicyRequest(token_fetcher_->GetDeviceToken(),
                                   token_fetcher_->GetDeviceID(),
                                   policy_request, this);
  }
}

void DeviceManagementPolicyProvider::RefreshTaskExecute() {
  DCHECK(refresh_task_pending_);
  refresh_task_pending_ = false;
  // If there is no valid device token, the token_fetcher_ apparently failed,
  // so it must be restarted.
  if (!token_fetcher_->IsTokenValid()) {
    if (token_fetcher_->IsTokenPending()) {
      NOTREACHED();
      return;
    }
    token_fetcher_->Restart();
    return;
  }
  // If there is a device token, just refresh policies.
  SendPolicyRequest();
}

void DeviceManagementPolicyProvider::ScheduleRefreshTask(
    int64 delay_in_milliseconds) {
  // This check is simply a safeguard, the situation currently cannot happen.
  if (refresh_task_pending_) {
    NOTREACHED();
    return;
  }
  refresh_task_pending_ = true;
  BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
                                 new RefreshTask(AsWeakPtr()),
                                 delay_in_milliseconds);
}

int64 DeviceManagementPolicyProvider::GetRefreshTaskDelay() {
  int64 delay = policy_refresh_rate_ms_;
  if (policy_refresh_max_earlier_ms_)
    delay -= base::RandGenerator(policy_refresh_max_earlier_ms_);
  return delay;
}

FilePath DeviceManagementPolicyProvider::GetTokenPath() {
  return storage_dir_.Append(FILE_PATH_LITERAL("Token"));
}

void DeviceManagementPolicyProvider::SetDeviceTokenFetcher(
    DeviceTokenFetcher* token_fetcher) {
  DCHECK(!token_fetcher_);
  token_fetcher_ = token_fetcher;
}

// static
std::string DeviceManagementPolicyProvider::GetDeviceManagementURL() {
  return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kDeviceManagementUrl);
}

// static
FilePath DeviceManagementPolicyProvider::GetOrCreateDeviceManagementDir(
    const FilePath& user_data_dir) {
  const FilePath device_management_dir = user_data_dir.Append(
      FILE_PATH_LITERAL("Device Management"));
  if (!file_util::DirectoryExists(device_management_dir)) {
    if (!file_util::CreateDirectory(device_management_dir))
      NOTREACHED();
  }
  return device_management_dir;
}

}  // namespace policy
