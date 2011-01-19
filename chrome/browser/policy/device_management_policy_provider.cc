// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_policy_provider.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/task.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/device_management_policy_cache.h"
#include "chrome/browser/policy/profile_policy_context.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace policy {

namespace em = enterprise_management;

// The maximum ratio in percent of the policy refresh rate we use for adjusting
// the policy refresh time instant. The rationale is to avoid load spikes from
// many devices that were set up in sync for some reason.
const int kPolicyRefreshDeviationFactorPercent = 10;
// Maximum deviation we are willing to accept.
const int64 kPolicyRefreshDeviationMaxInMilliseconds = 30 * 60 * 1000;

// These are the base values for delays before retrying after an error. They
// will be doubled each time they are used.
const int64 kPolicyRefreshErrorDelayInMilliseconds = 3 * 1000;  // 3 seconds
const int64 kDeviceTokenRefreshErrorDelayInMilliseconds = 3 * 1000;
// For unmanaged devices, check once per day whether they're still unmanaged.
const int64 kPolicyRefreshUnmanagedDeviceInMilliseconds = 24 * 60 * 60 * 1000;

const FilePath::StringType kDeviceTokenFilename = FILE_PATH_LITERAL("Token");
const FilePath::StringType kPolicyFilename = FILE_PATH_LITERAL("Policy");

// Calls back into the provider to refresh policy.
class DeviceManagementPolicyProvider::RefreshTask : public CancelableTask {
 public:
  explicit RefreshTask(DeviceManagementPolicyProvider* provider)
      : provider_(provider) {}

  // Task implementation:
  virtual void Run() {
    if (provider_)
      provider_->RefreshTaskExecute();
  }

  // CancelableTask implementation:
  virtual void Cancel() {
    provider_ = NULL;
  }

 private:
  DeviceManagementPolicyProvider* provider_;
};

DeviceManagementPolicyProvider::DeviceManagementPolicyProvider(
    const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list,
    DeviceManagementBackend* backend,
    Profile* profile)
    : ConfigurationPolicyProvider(policy_list) {
  Initialize(backend,
             profile,
             ProfilePolicyContext::kDefaultPolicyRefreshRateInMilliseconds,
             kPolicyRefreshDeviationFactorPercent,
             kPolicyRefreshDeviationMaxInMilliseconds,
             kPolicyRefreshErrorDelayInMilliseconds,
             kDeviceTokenRefreshErrorDelayInMilliseconds,
             kPolicyRefreshUnmanagedDeviceInMilliseconds);
}

DeviceManagementPolicyProvider::~DeviceManagementPolicyProvider() {
  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_,
                    OnProviderGoingAway());
  CancelRefreshTask();
}

bool DeviceManagementPolicyProvider::Provide(
    ConfigurationPolicyStoreInterface* policy_store) {
  scoped_ptr<DictionaryValue> policies(cache_->GetPolicy());
  DecodePolicyValueTree(policies.get(), policy_store);
  return true;
}

bool DeviceManagementPolicyProvider::IsInitializationComplete() const {
  return !cache_->last_policy_refresh_time().is_null();
}

void DeviceManagementPolicyProvider::HandlePolicyResponse(
    const em::DevicePolicyResponse& response) {
  DCHECK(TokenAvailable());
  if (cache_->SetPolicy(response)) {
    initial_fetch_done_ = true;
    NotifyCloudPolicyUpdate();
  }
  SetState(STATE_POLICY_VALID);
}

void DeviceManagementPolicyProvider::OnError(
    DeviceManagementBackend::ErrorCode code) {
  DCHECK(TokenAvailable());
  if (code == DeviceManagementBackend::kErrorServiceDeviceNotFound ||
      code == DeviceManagementBackend::kErrorServiceManagementTokenInvalid) {
    LOG(WARNING) << "The device token was either invalid or unknown to the "
                 << "device manager, re-registering device.";
    SetState(STATE_TOKEN_RESET);
  } else if (code ==
             DeviceManagementBackend::kErrorServiceManagementNotSupported) {
    VLOG(1) << "The device is no longer managed, resetting device token.";
    SetState(STATE_TOKEN_RESET);
  } else {
    LOG(WARNING) << "Could not provide policy from the device manager (error = "
                 << code << "), will retry in "
                 << (effective_policy_refresh_error_delay_ms_ / 1000)
                 << " seconds.";
    SetState(STATE_POLICY_ERROR);
  }
}

void DeviceManagementPolicyProvider::OnTokenSuccess() {
  DCHECK(!TokenAvailable());
  SetState(STATE_TOKEN_VALID);
}

void DeviceManagementPolicyProvider::OnTokenError() {
  DCHECK(!TokenAvailable());
  LOG(WARNING) << "Could not retrieve device token.";
  SetState(STATE_TOKEN_ERROR);
}

void DeviceManagementPolicyProvider::OnNotManaged() {
  DCHECK(!TokenAvailable());
  VLOG(1) << "This device is not managed.";
  cache_->SetDeviceUnmanaged();
  SetState(STATE_UNMANAGED);
}

void DeviceManagementPolicyProvider::SetRefreshRate(
    int64 refresh_rate_milliseconds) {
  policy_refresh_rate_ms_ = refresh_rate_milliseconds;

  // Reschedule the refresh task if necessary.
  if (state_ == STATE_POLICY_VALID)
    SetState(STATE_POLICY_VALID);
}

DeviceManagementPolicyProvider::DeviceManagementPolicyProvider(
    const PolicyDefinitionList* policy_list,
    DeviceManagementBackend* backend,
    Profile* profile,
    int64 policy_refresh_rate_ms,
    int policy_refresh_deviation_factor_percent,
    int64 policy_refresh_deviation_max_ms,
    int64 policy_refresh_error_delay_ms,
    int64 token_fetch_error_delay_ms,
    int64 unmanaged_device_refresh_rate_ms)
    : ConfigurationPolicyProvider(policy_list) {
  Initialize(backend,
             profile,
             policy_refresh_rate_ms,
             policy_refresh_deviation_factor_percent,
             policy_refresh_deviation_max_ms,
             policy_refresh_error_delay_ms,
             token_fetch_error_delay_ms,
             unmanaged_device_refresh_rate_ms);
}

void DeviceManagementPolicyProvider::Initialize(
    DeviceManagementBackend* backend,
    Profile* profile,
    int64 policy_refresh_rate_ms,
    int policy_refresh_deviation_factor_percent,
    int64 policy_refresh_deviation_max_ms,
    int64 policy_refresh_error_delay_ms,
    int64 token_fetch_error_delay_ms,
    int64 unmanaged_device_refresh_rate_ms) {
  DCHECK(profile);
  backend_.reset(backend);
  profile_ = profile;
  storage_dir_ = GetOrCreateDeviceManagementDir(profile_->GetPath());
  state_ = STATE_INITIALIZING;
  initial_fetch_done_ = false;
  refresh_task_ = NULL;
  policy_refresh_rate_ms_ = policy_refresh_rate_ms;
  policy_refresh_deviation_factor_percent_ =
      policy_refresh_deviation_factor_percent;
  policy_refresh_deviation_max_ms_ = policy_refresh_deviation_max_ms;
  policy_refresh_error_delay_ms_ = policy_refresh_error_delay_ms;
  effective_policy_refresh_error_delay_ms_ = policy_refresh_error_delay_ms;
  token_fetch_error_delay_ms_ = token_fetch_error_delay_ms;
  effective_token_fetch_error_delay_ms_ = token_fetch_error_delay_ms;
  unmanaged_device_refresh_rate_ms_ = unmanaged_device_refresh_rate_ms;

  const FilePath policy_path = storage_dir_.Append(kPolicyFilename);
  cache_.reset(new DeviceManagementPolicyCache(policy_path));
  cache_->LoadPolicyFromFile();

  SetDeviceTokenFetcher(new DeviceTokenFetcher(backend_.get(), profile,
                                               GetTokenPath()));

  if (cache_->is_device_unmanaged()) {
    // This is a non-first login on an unmanaged device.
    SetState(STATE_UNMANAGED);
  } else {
    SetState(STATE_INITIALIZING);
  }
}

void DeviceManagementPolicyProvider::AddObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DeviceManagementPolicyProvider::RemoveObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DeviceManagementPolicyProvider::SendPolicyRequest() {
  em::DevicePolicyRequest policy_request;
  policy_request.set_policy_scope(kChromePolicyScope);
  em::DevicePolicySettingRequest* setting =
      policy_request.add_setting_request();
  setting->set_key(kChromeDevicePolicySettingKey);
  setting->set_watermark("");
  backend_->ProcessPolicyRequest(token_fetcher_->GetDeviceToken(),
                                 token_fetcher_->GetDeviceID(),
                                 policy_request, this);
}

void DeviceManagementPolicyProvider::RefreshTaskExecute() {
  DCHECK(refresh_task_);
  refresh_task_ = NULL;

  switch (state_) {
    case STATE_INITIALIZING:
      token_fetcher_->StartFetching();
      return;
    case STATE_TOKEN_VALID:
    case STATE_POLICY_VALID:
    case STATE_POLICY_ERROR:
      SendPolicyRequest();
      return;
    case STATE_UNMANAGED:
    case STATE_TOKEN_ERROR:
    case STATE_TOKEN_RESET:
      token_fetcher_->Restart();
      return;
  }

  NOTREACHED() << "Unhandled state";
}

void DeviceManagementPolicyProvider::CancelRefreshTask() {
  if (refresh_task_) {
    refresh_task_->Cancel();
    refresh_task_ = NULL;
  }
}

void DeviceManagementPolicyProvider::NotifyCloudPolicyUpdate() {
  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_,
                    OnUpdatePolicy());
}

FilePath DeviceManagementPolicyProvider::GetTokenPath() {
  return storage_dir_.Append(kDeviceTokenFilename);
}

void DeviceManagementPolicyProvider::SetDeviceTokenFetcher(
    DeviceTokenFetcher* token_fetcher) {
  registrar_.Init(token_fetcher);
  registrar_.AddObserver(this);
  token_fetcher_ = token_fetcher;
}

void DeviceManagementPolicyProvider::SetState(
    DeviceManagementPolicyProvider::ProviderState new_state) {
  state_ = new_state;

  // If this state transition completes the initial policy fetch, let the
  // observers now.
  if (!initial_fetch_done_ &&
      new_state != STATE_INITIALIZING &&
      new_state != STATE_TOKEN_VALID) {
    initial_fetch_done_ = true;
    NotifyCloudPolicyUpdate();
  }

  base::Time now(base::Time::NowFromSystemTime());
  base::Time refresh_at;
  base::Time last_refresh(cache_->last_policy_refresh_time());
  if (last_refresh.is_null())
    last_refresh = now;

  // Determine when to take the next step.
  switch (state_) {
    case STATE_INITIALIZING:
      refresh_at = now;
      break;
    case STATE_TOKEN_VALID:
      effective_token_fetch_error_delay_ms_ = token_fetch_error_delay_ms_;
      refresh_at = now;
      break;
    case STATE_TOKEN_RESET:
      refresh_at = now;
      break;
    case STATE_UNMANAGED:
      refresh_at = last_refresh +
          base::TimeDelta::FromMilliseconds(unmanaged_device_refresh_rate_ms_);
      break;
    case STATE_POLICY_VALID:
      effective_policy_refresh_error_delay_ms_ = policy_refresh_error_delay_ms_;
      refresh_at =
          last_refresh + base::TimeDelta::FromMilliseconds(GetRefreshDelay());
      break;
    case STATE_TOKEN_ERROR:
      refresh_at = now + base::TimeDelta::FromMilliseconds(
                             effective_token_fetch_error_delay_ms_);
      effective_token_fetch_error_delay_ms_ *= 2;
      if (effective_token_fetch_error_delay_ms_ > policy_refresh_rate_ms_)
        effective_token_fetch_error_delay_ms_ = policy_refresh_rate_ms_;
      break;
    case STATE_POLICY_ERROR:
      refresh_at = now + base::TimeDelta::FromMilliseconds(
                             effective_policy_refresh_error_delay_ms_);
      effective_policy_refresh_error_delay_ms_ *= 2;
      if (effective_policy_refresh_error_delay_ms_ > policy_refresh_rate_ms_)
        effective_policy_refresh_error_delay_ms_ = policy_refresh_rate_ms_;
      break;
  }

  // Update the refresh task.
  CancelRefreshTask();
  if (!refresh_at.is_null()) {
    refresh_task_ = new RefreshTask(this);
    int64 delay = std::max<int64>((refresh_at - now).InMilliseconds(), 0);
    BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE, refresh_task_,
                                   delay);
  }
}

int64 DeviceManagementPolicyProvider::GetRefreshDelay() {
  int64 deviation = (policy_refresh_deviation_factor_percent_ *
                     policy_refresh_rate_ms_) / 100;
  deviation = std::min(deviation, policy_refresh_deviation_max_ms_);
  return policy_refresh_rate_ms_ - base::RandGenerator(deviation + 1);
}

bool DeviceManagementPolicyProvider::TokenAvailable() const {
  return state_ == STATE_TOKEN_VALID ||
         state_ == STATE_POLICY_VALID ||
         state_ == STATE_POLICY_ERROR;
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
