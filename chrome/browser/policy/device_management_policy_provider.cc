// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_policy_provider.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/device_management_backend_impl.h"
#include "chrome/browser/policy/device_management_policy_cache.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace {

const char kChromePolicyScope[] = "cros/device";
const char kChromeDevicePolicySettingKey[] = "chrome-policy";
const int64 kPolicyRefreshRateInMinutes = 3 * 60;  // 3 hours

}  // namespace

namespace policy {

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

DeviceManagementPolicyProvider::DeviceManagementPolicyProvider(
    const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list,
    DeviceManagementBackend* backend,
    const FilePath& storage_dir)
    : ConfigurationPolicyProvider(policy_list),
      backend_(backend),
      storage_dir_(GetOrCreateDeviceManagementDir(storage_dir)),
      policy_request_pending_(false) {
  Initialize();
}

DeviceManagementPolicyProvider::DeviceManagementPolicyProvider(
    const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list)
    : ConfigurationPolicyProvider(policy_list),
      policy_request_pending_(false) {
  FilePath user_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_dir))
    NOTREACHED();
  storage_dir_ = GetOrCreateDeviceManagementDir(user_dir);
  Initialize();
}

DeviceManagementPolicyProvider::~DeviceManagementPolicyProvider() {}

bool DeviceManagementPolicyProvider::Provide(
    ConfigurationPolicyStoreInterface* policy_store) {
  scoped_ptr<DictionaryValue> policies(cache_->GetPolicy());
  DecodePolicyValueTree(policies.get(), policy_store);
  return true;
}

void DeviceManagementPolicyProvider::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if ((type == NotificationType::DEVICE_TOKEN_AVAILABLE) &&
      (token_fetcher_.get() == Source<DeviceTokenFetcher>(source).ptr())) {
    if (!policy_request_pending_) {
      if (IsPolicyStale())
        SendPolicyRequest();
    }
  } else {
    NOTREACHED();
  }
}

void DeviceManagementPolicyProvider::HandlePolicyResponse(
    const em::DevicePolicyResponse& response) {
  cache_->SetPolicy(response);
  NotifyStoreOfPolicyChange();
  policy_request_pending_ = false;
}

void DeviceManagementPolicyProvider::OnError(
    DeviceManagementBackend::ErrorCode code) {
  LOG(WARNING) << "could not provide policy from the device manager (error = "
               << code << ")";
  policy_request_pending_ = false;
  // TODO(danno): do something sensible in the error case, perhaps retry later?
}

DeviceManagementBackend* DeviceManagementPolicyProvider::GetBackend() {
  if (!backend_.get()) {
    backend_.reset(new DeviceManagementBackendImpl(
        GetDeviceManagementURL()));
  }
  return backend_.get();
}

void DeviceManagementPolicyProvider::Initialize() {
  registrar_.Add(this,
                 NotificationType::DEVICE_TOKEN_AVAILABLE,
                 NotificationService::AllSources());

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
  const FilePath token_path = storage_dir_.Append(
      FILE_PATH_LITERAL("Token"));
  token_fetcher_ = new DeviceTokenFetcher(GetBackend(), token_path);
  token_fetcher_->StartFetching();
}

void DeviceManagementPolicyProvider::SendPolicyRequest() {
  if (!policy_request_pending_) {
    em::DevicePolicyRequest policy_request;
    policy_request.set_policy_scope(kChromePolicyScope);
    em::DevicePolicySettingRequest* setting =
        policy_request.add_setting_request();
    setting->set_key(kChromeDevicePolicySettingKey);
    GetBackend()->ProcessPolicyRequest(token_fetcher_->GetDeviceToken(),
                                       policy_request,
                                       this);
    policy_request_pending_ = true;
  }
}

bool DeviceManagementPolicyProvider::IsPolicyStale() const {
  base::Time now(base::Time::NowFromSystemTime());
  base::Time last_policy_refresh_time =
      cache_->last_policy_refresh_time();
  base::Time policy_expiration_time =
      last_policy_refresh_time + base::TimeDelta::FromMinutes(
          kPolicyRefreshRateInMinutes);
  return (now > policy_expiration_time);
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
