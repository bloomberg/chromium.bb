// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include <limits>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"
#include "chrome/browser/policy/app_pack_updater.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#include "policy/policy_constants.h"

using google::protobuf::RepeatedPtrField;

namespace em = enterprise_management;

namespace {

// Stores policy, updates the owner key if required and reports the status
// through a callback.
class StorePolicyOperation : public chromeos::OwnerManager::KeyUpdateDelegate {
 public:
  typedef base::Callback<void(chromeos::SignedSettings::ReturnCode)> Callback;

  StorePolicyOperation(chromeos::SignedSettingsHelper* signed_settings_helper,
                       const em::PolicyFetchResponse& policy,
                       const Callback& callback)
      : signed_settings_helper_(signed_settings_helper),
        policy_(policy),
        callback_(callback),
        weak_ptr_factory_(this) {
    signed_settings_helper_->StartStorePolicyOp(
        policy,
        base::Bind(&StorePolicyOperation::OnStorePolicyCompleted,
                   weak_ptr_factory_.GetWeakPtr()));
  }
  virtual ~StorePolicyOperation() {
  }

  void OnStorePolicyCompleted(chromeos::SignedSettings::ReturnCode code) {
    if (code != chromeos::SignedSettings::SUCCESS) {
      callback_.Run(code);
      delete this;
      return;
    }

    if (policy_.has_new_public_key()) {
      // The session manager has successfully done a key rotation. Replace the
      // owner key also in chrome.
      const std::string& new_key = policy_.new_public_key();
      const std::vector<uint8> new_key_data(new_key.c_str(),
                                            new_key.c_str() + new_key.size());
      chromeos::OwnershipService::GetSharedInstance()->StartUpdateOwnerKey(
          new_key_data, this);
      return;
    } else {
      chromeos::CrosSettings::Get()->ReloadProviders();
      callback_.Run(chromeos::SignedSettings::SUCCESS);
      delete this;
      return;
    }
  }

  // OwnerManager::KeyUpdateDelegate implementation:
  virtual void OnKeyUpdated() OVERRIDE {
    chromeos::CrosSettings::Get()->ReloadProviders();
    callback_.Run(chromeos::SignedSettings::SUCCESS);
    delete this;
  }

 private:

  chromeos::SignedSettingsHelper* signed_settings_helper_;
  em::PolicyFetchResponse policy_;
  Callback callback_;

  base::WeakPtrFactory<StorePolicyOperation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(StorePolicyOperation);
};

// Decodes a protobuf integer to an IntegerValue. The caller assumes ownership
// of the return Value*. Returns NULL in case the input value is out of bounds.
Value* DecodeIntegerValue(google::protobuf::int64 value) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return NULL;
  }

  return Value::CreateIntegerValue(static_cast<int>(value));
}

}  // namespace

namespace policy {

DevicePolicyCache::DevicePolicyCache(
    CloudPolicyDataStore* data_store,
    EnterpriseInstallAttributes* install_attributes)
    : data_store_(data_store),
      install_attributes_(install_attributes),
      signed_settings_helper_(chromeos::SignedSettingsHelper::Get()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      policy_fetch_pending_(false) {
}

DevicePolicyCache::DevicePolicyCache(
    CloudPolicyDataStore* data_store,
    EnterpriseInstallAttributes* install_attributes,
    chromeos::SignedSettingsHelper* signed_settings_helper)
    : data_store_(data_store),
      install_attributes_(install_attributes),
      signed_settings_helper_(signed_settings_helper),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      policy_fetch_pending_(false) {
}

DevicePolicyCache::~DevicePolicyCache() {
}

void DevicePolicyCache::Load() {
  signed_settings_helper_->StartRetrievePolicyOp(
      base::Bind(&DevicePolicyCache::OnRetrievePolicyCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool DevicePolicyCache::SetPolicy(const em::PolicyFetchResponse& policy) {
  DCHECK(IsReady());

  // Make sure we have an enterprise device.
  std::string registration_user(install_attributes_->GetRegistrationUser());
  if (registration_user.empty()) {
    LOG(WARNING) << "Refusing to accept policy on non-enterprise device.";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy,
                              kMetricPolicyFetchNonEnterpriseDevice,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return false;
  }

  // Check the user this policy is for against the device-locked name.
  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(policy.policy_data())) {
    LOG(WARNING) << "Invalid policy protobuf";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchInvalidPolicy,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return false;
  }

  // Existing installations may not have a canonicalized version of the
  // registration user name in install attributes, so re-canonicalize here.
  if (chromeos::Authenticator::Canonicalize(registration_user) !=
      chromeos::Authenticator::Canonicalize(policy_data.username())) {
    LOG(WARNING) << "Refusing policy blob for " << policy_data.username()
                 << " which doesn't match " << registration_user;
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchUserMismatch,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return false;
  }

  set_last_policy_refresh_time(base::Time::NowFromSystemTime());

  // Start a store operation.
  StorePolicyOperation::Callback callback =
      base::Bind(&DevicePolicyCache::PolicyStoreOpCompleted,
                 weak_ptr_factory_.GetWeakPtr());
  new StorePolicyOperation(signed_settings_helper_, policy, callback);
  policy_fetch_pending_ = true;
  return true;
}

void DevicePolicyCache::SetUnmanaged() {
  LOG(WARNING) << "Tried to set DevicePolicyCache to 'unmanaged'!";
  // This is not supported for DevicePolicyCache.
}

void DevicePolicyCache::SetFetchingDone() {
  // Don't send the notification just yet if there is a pending policy
  // store/reload cycle.
  if (!policy_fetch_pending_)
    CloudPolicyCacheBase::SetFetchingDone();
}

void DevicePolicyCache::OnRetrievePolicyCompleted(
    chromeos::SignedSettings::ReturnCode code,
    const em::PolicyFetchResponse& policy) {
  DCHECK(CalledOnValidThread());
  if (!IsReady()) {
    std::string device_token;
    InstallInitialPolicy(code, policy, &device_token);
    SetTokenAndFlagReady(device_token);
  } else {  // In other words, IsReady() == true
    if (code != chromeos::SignedSettings::SUCCESS) {
      if (code == chromeos::SignedSettings::BAD_SIGNATURE) {
        UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchBadSignature,
                                  kMetricPolicySize);
        InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                       CloudPolicySubsystem::SIGNATURE_MISMATCH);
      } else {
        UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchOtherFailed,
                                  kMetricPolicySize);
        InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                       CloudPolicySubsystem::POLICY_LOCAL_ERROR);
      }
    } else {
      bool ok = SetPolicyInternal(policy, NULL, false);
      if (ok) {
        UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchOK,
                                  kMetricPolicySize);
      }
    }
  }
  CheckFetchingDone();
}

bool DevicePolicyCache::DecodePolicyData(const em::PolicyData& policy_data,
                                         PolicyMap* policies) {
  em::ChromeDeviceSettingsProto policy;
  if (!policy.ParseFromString(policy_data.policy_value())) {
    LOG(WARNING) << "Failed to parse ChromeDeviceSettingsProto.";
    return false;
  }
  DecodeDevicePolicy(policy, policies);
  return true;
}

void DevicePolicyCache::PolicyStoreOpCompleted(
    chromeos::SignedSettings::ReturnCode code) {
  DCHECK(CalledOnValidThread());
  if (code != chromeos::SignedSettings::SUCCESS) {
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyStoreFailed,
                              kMetricPolicySize);
    if (code == chromeos::SignedSettings::BAD_SIGNATURE) {
      UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchBadSignature,
                                kMetricPolicySize);
      InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                     CloudPolicySubsystem::SIGNATURE_MISMATCH);
    } else {
      UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchOtherFailed,
                                kMetricPolicySize);
      InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                     CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    }
    CheckFetchingDone();
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyStoreSucceeded,
                            kMetricPolicySize);
  signed_settings_helper_->StartRetrievePolicyOp(
      base::Bind(&DevicePolicyCache::OnRetrievePolicyCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DevicePolicyCache::InstallInitialPolicy(
    chromeos::SignedSettings::ReturnCode code,
    const em::PolicyFetchResponse& policy,
    std::string* device_token) {
  if (code == chromeos::SignedSettings::NOT_FOUND ||
      code == chromeos::SignedSettings::KEY_UNAVAILABLE ||
      !policy.has_policy_data()) {
    InformNotifier(CloudPolicySubsystem::UNENROLLED,
                   CloudPolicySubsystem::NO_DETAILS);
    return;
  }
  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(policy.policy_data())) {
    LOG(WARNING) << "Failed to parse PolicyData protobuf.";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyLoadFailed,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return;
  }
  if (!policy_data.has_request_token() ||
      policy_data.request_token().empty()) {
    SetUnmanagedInternal(base::Time::NowFromSystemTime());
    InformNotifier(CloudPolicySubsystem::UNMANAGED,
                   CloudPolicySubsystem::NO_DETAILS);
    // TODO(jkummerow): Reminder: When we want to feed device-wide settings
    // made by a local owner into this cache, we need to call
    // SetPolicyInternal() here.
    return;
  }
  if (!policy_data.has_username() || !policy_data.has_device_id()) {
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyLoadFailed,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyLoadSucceeded,
                            kMetricPolicySize);
  data_store_->set_user_name(policy_data.username());
  data_store_->set_device_id(policy_data.device_id());
  *device_token = policy_data.request_token();
  base::Time timestamp;
  if (SetPolicyInternal(policy, &timestamp, true))
    set_last_policy_refresh_time(timestamp);
}

void DevicePolicyCache::SetTokenAndFlagReady(const std::string& device_token) {
  // Wait for device settings to become available.
  if (!chromeos::CrosSettings::Get()->PrepareTrustedValues(
          base::Bind(&DevicePolicyCache::SetTokenAndFlagReady,
                     weak_ptr_factory_.GetWeakPtr(),
                     device_token))) {
    return;
  }

  // We need to call SetDeviceToken unconditionally to indicate the cache has
  // finished loading.
  data_store_->SetDeviceToken(device_token, true);
  SetReady();
}

void DevicePolicyCache::CheckFetchingDone() {
  if (policy_fetch_pending_) {
    CloudPolicyCacheBase::SetFetchingDone();
    policy_fetch_pending_ = false;
  }
}

void DevicePolicyCache::DecodeDevicePolicy(
    const em::ChromeDeviceSettingsProto& policy,
    PolicyMap* policies) {
  // Decode the various groups of policies.
  DecodeLoginPolicies(policy, policies);
  DecodeKioskPolicies(policy, policies, install_attributes_);
  DecodeNetworkPolicies(policy, policies, install_attributes_);
  DecodeReportingPolicies(policy, policies);
  DecodeGenericPolicies(policy, policies);
}

// static
void DevicePolicyCache::DecodeLoginPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PolicyMap* policies) {
  if (policy.has_guest_mode_enabled()) {
    const em::GuestModeEnabledProto& container(policy.guest_mode_enabled());
    if (container.has_guest_mode_enabled()) {
      policies->Set(key::kDeviceGuestModeEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(container.guest_mode_enabled()));
    }
  }

  if (policy.has_show_user_names()) {
    const em::ShowUserNamesOnSigninProto& container(policy.show_user_names());
    if (container.has_show_user_names()) {
      policies->Set(key::kDeviceShowUserNamesOnSignin,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(container.show_user_names()));
    }
  }

  if (policy.has_allow_new_users()) {
    const em::AllowNewUsersProto& container(policy.allow_new_users());
    if (container.has_allow_new_users()) {
      policies->Set(key::kDeviceAllowNewUsers,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(container.allow_new_users()));
    }
  }

  if (policy.has_user_whitelist()) {
    const em::UserWhitelistProto& container(policy.user_whitelist());
    if (container.user_whitelist_size()) {
      ListValue* whitelist = new ListValue();
      RepeatedPtrField<std::string>::const_iterator entry;
      for (entry = container.user_whitelist().begin();
           entry != container.user_whitelist().end();
           ++entry) {
        whitelist->Append(Value::CreateStringValue(*entry));
      }
      policies->Set(key::kDeviceUserWhitelist,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    whitelist);
    }
  }

  if (policy.has_ephemeral_users_enabled()) {
    const em::EphemeralUsersEnabledProto& container(
        policy.ephemeral_users_enabled());
    if (container.has_ephemeral_users_enabled()) {
      policies->Set(key::kDeviceEphemeralUsersEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(
                        container.ephemeral_users_enabled()));
    }
  }
}

// static
void DevicePolicyCache::DecodeKioskPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PolicyMap* policies,
    EnterpriseInstallAttributes* install_attributes) {
  // No policies if this is not KIOSK.
  if (install_attributes->GetMode() != DEVICE_MODE_KIOSK)
    return;

  if (policy.has_forced_logout_timeouts()) {
    const em::ForcedLogoutTimeoutsProto& container(
        policy.forced_logout_timeouts());
    if (container.has_idle_logout_timeout()) {
      policies->Set(key::kDeviceIdleLogoutTimeout,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    DecodeIntegerValue(container.idle_logout_timeout()));
    }
    if (container.has_idle_logout_warning_duration()) {
      policies->Set(key::kDeviceIdleLogoutWarningDuration,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    DecodeIntegerValue(
                        container.idle_logout_warning_duration()));
    }
  }

  if (policy.has_login_screen_saver()) {
    const em::ScreenSaverProto& container(
        policy.login_screen_saver());
    if (container.has_screen_saver_extension_id()) {
      policies->Set(key::kDeviceLoginScreenSaverId,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateStringValue(
                        container.screen_saver_extension_id()));
    }
    if (container.has_screen_saver_timeout()) {
      policies->Set(key::kDeviceLoginScreenSaverTimeout,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    DecodeIntegerValue(container.screen_saver_timeout()));
    }
  }

  if (policy.has_app_pack()) {
    const em::AppPackProto& container(policy.app_pack());
    base::ListValue* app_pack_list = new base::ListValue();
    for (int i = 0; i < container.app_pack_size(); ++i) {
      const em::AppPackEntryProto& entry(container.app_pack(i));
      if (entry.has_extension_id() && entry.has_update_url()) {
        base::DictionaryValue* dict = new base::DictionaryValue();
        dict->SetString(AppPackUpdater::kExtensionId, entry.extension_id());
        dict->SetString(AppPackUpdater::kUpdateUrl, entry.update_url());
        app_pack_list->Append(dict);
      }
    }
    policies->Set(key::kDeviceAppPack,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE,
                  app_pack_list);
  }

  if (policy.has_pinned_apps()) {
    const em::PinnedAppsProto& container(policy.pinned_apps());
    base::ListValue* pinned_apps_list = new base::ListValue();
    for (int i = 0; i < container.app_id_size(); ++i)
      pinned_apps_list->Append(Value::CreateStringValue(container.app_id(i)));

    policies->Set(key::kPinnedLauncherApps,
                  POLICY_LEVEL_RECOMMENDED,
                  POLICY_SCOPE_MACHINE,
                  pinned_apps_list);
  }
}

// static
void DevicePolicyCache::DecodeNetworkPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PolicyMap* policies,
    EnterpriseInstallAttributes* install_attributes) {
  if (policy.has_device_proxy_settings()) {
    const em::DeviceProxySettingsProto& container(
        policy.device_proxy_settings());
    scoped_ptr<DictionaryValue> proxy_settings(new DictionaryValue);
    if (container.has_proxy_mode())
      proxy_settings->SetString(key::kProxyMode, container.proxy_mode());
    if (container.has_proxy_server())
      proxy_settings->SetString(key::kProxyServer, container.proxy_server());
    if (container.has_proxy_pac_url())
      proxy_settings->SetString(key::kProxyPacUrl, container.proxy_pac_url());
    if (container.has_proxy_bypass_list()) {
      proxy_settings->SetString(key::kProxyBypassList,
                                container.proxy_bypass_list());
    }

    // Figure out the level. Proxy policy is mandatory in kiosk mode.
    PolicyLevel level = POLICY_LEVEL_RECOMMENDED;
    if (install_attributes->GetMode() == DEVICE_MODE_KIOSK)
      level = POLICY_LEVEL_MANDATORY;

    if (!proxy_settings->empty()) {
      policies->Set(key::kProxySettings,
                    level,
                    POLICY_SCOPE_MACHINE,
                    proxy_settings.release());
    }
  }

  if (policy.has_data_roaming_enabled()) {
    const em::DataRoamingEnabledProto& container(policy.data_roaming_enabled());
    if (container.has_data_roaming_enabled()) {
      policies->Set(key::kDeviceDataRoamingEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(
                        container.data_roaming_enabled()));
    }
  }

  if (policy.has_open_network_configuration() &&
      policy.open_network_configuration().has_open_network_configuration()) {
    std::string config(
        policy.open_network_configuration().open_network_configuration());
    policies->Set(key::kDeviceOpenNetworkConfiguration,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE,
                  Value::CreateStringValue(config));
  }
}

// static
void DevicePolicyCache::DecodeReportingPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PolicyMap* policies) {
  if (policy.has_device_reporting()) {
    const em::DeviceReportingProto& container(policy.device_reporting());
    if (container.has_report_version_info()) {
      policies->Set(key::kReportDeviceVersionInfo,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(container.report_version_info()));
    }
    if (container.has_report_activity_times()) {
      policies->Set(key::kReportDeviceActivityTimes,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(
                        container.report_activity_times()));
    }
    if (container.has_report_boot_mode()) {
      policies->Set(key::kReportDeviceBootMode,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(container.report_boot_mode()));
    }
    if (container.has_report_location()) {
      policies->Set(key::kReportDeviceLocation,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(container.report_location()));
    }
  }
}

// static
void DevicePolicyCache::DecodeGenericPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PolicyMap* policies) {
  if (policy.has_device_policy_refresh_rate()) {
    const em::DevicePolicyRefreshRateProto& container(
        policy.device_policy_refresh_rate());
    if (container.has_device_policy_refresh_rate()) {
      policies->Set(key::kDevicePolicyRefreshRate,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    DecodeIntegerValue(container.device_policy_refresh_rate()));
    }
  }

  if (policy.has_metrics_enabled()) {
    const em::MetricsEnabledProto& container(policy.metrics_enabled());
    if (container.has_metrics_enabled()) {
      policies->Set(key::kDeviceMetricsReportingEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(container.metrics_enabled()));
    }
  }

  if (policy.has_release_channel()) {
    const em::ReleaseChannelProto& container(policy.release_channel());
    if (container.has_release_channel()) {
      std::string channel(container.release_channel());
      policies->Set(key::kChromeOsReleaseChannel,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateStringValue(channel));
      // TODO(dubroy): Once http://crosbug.com/17015 is implemented, we won't
      // have to pass the channel in here, only ping the update engine to tell
      // it to fetch the channel from the policy.
      chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->
          SetReleaseTrack(channel);
    }
    if (container.has_release_channel_delegated()) {
      policies->Set(key::kChromeOsReleaseChannelDelegated,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(
                        container.release_channel_delegated()));
    }
  }

  if (policy.has_auto_update_settings()) {
    const em::AutoUpdateSettingsProto& container(policy.auto_update_settings());
    if (container.has_update_disabled()) {
      policies->Set(key::kDeviceAutoUpdateDisabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(container.update_disabled()));
    }

    if (container.has_target_version_prefix()) {
      policies->Set(key::kDeviceTargetVersionPrefix,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateStringValue(
                        container.target_version_prefix()));
    }
  }

  if (policy.has_start_up_urls()) {
    const em::StartUpUrlsProto& container(policy.start_up_urls());
    if (container.start_up_urls_size()) {
      ListValue* urls = new ListValue();
      RepeatedPtrField<std::string>::const_iterator entry;
      for (entry = container.start_up_urls().begin();
           entry != container.start_up_urls().end();
           ++entry) {
        urls->Append(Value::CreateStringValue(*entry));
      }
      policies->Set(key::kDeviceStartUpUrls,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    urls);
    }
  }
}

}  // namespace policy
