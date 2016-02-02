// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_provider.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_cache.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "policy/proto/device_management_backend.pb.h"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

namespace em = enterprise_management;

namespace chromeos {

namespace {

// List of settings handled by the DeviceSettingsProvider.
const char* const kKnownSettings[] = {
    kAccountsPrefAllowGuest,
    kAccountsPrefAllowNewUser,
    kAccountsPrefDeviceLocalAccounts,
    kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
    kAccountsPrefDeviceLocalAccountAutoLoginDelay,
    kAccountsPrefDeviceLocalAccountAutoLoginId,
    kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline,
    kAccountsPrefEphemeralUsersEnabled,
    kAccountsPrefShowUserNamesOnSignIn,
    kAccountsPrefSupervisedUsersEnabled,
    kAccountsPrefTransferSAMLCookies,
    kAccountsPrefUsers,
    kAccountsPrefLoginScreenDomainAutoComplete,
    kAllowRedeemChromeOsRegistrationOffers,
    kAllowedConnectionTypesForUpdate,
    kAttestationForContentProtectionEnabled,
    kDeviceAttestationEnabled,
    kDeviceDisabled,
    kDeviceDisabledMessage,
    kDeviceOwner,
    kDisplayRotationDefault,
    kExtensionCacheSize,
    kHeartbeatEnabled,
    kHeartbeatFrequency,
    kSystemLogUploadEnabled,
    kPolicyMissingMitigationMode,
    kRebootOnShutdown,
    kReleaseChannel,
    kReleaseChannelDelegated,
    kReportDeviceActivityTimes,
    kReportDeviceBootMode,
    kReportDeviceHardwareStatus,
    kReportDeviceLocation,
    kReportDeviceNetworkInterfaces,
    kReportDeviceSessionStatus,
    kReportDeviceUsers,
    kReportDeviceVersionInfo,
    kReportUploadFrequency,
    kServiceAccountIdentity,
    kSignedDataRoamingEnabled,
    kStartUpFlags,
    kStatsReportingPref,
    kSystemTimezonePolicy,
    kSystemUse24HourClock,
    kUpdateDisabled,
    kVariationsRestrictParameter,
};

void DecodeLoginPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) {
  // For all our boolean settings the following is applicable:
  // true is default permissive value and false is safe prohibitive value.
  // Exceptions:
  //   kAccountsPrefEphemeralUsersEnabled has a default value of false.
  //   kAccountsPrefSupervisedUsersEnabled has a default value of false
  //     for enterprise devices and true for consumer devices.
  //   kAccountsPrefTransferSAMLCookies has a default value of false.
  if (policy.has_allow_new_users() &&
      policy.allow_new_users().has_allow_new_users()) {
    if (policy.allow_new_users().allow_new_users()) {
      // New users allowed, user whitelist ignored.
      new_values_cache->SetBoolean(kAccountsPrefAllowNewUser, true);
    } else {
      // New users not allowed, enforce user whitelist if present.
      new_values_cache->SetBoolean(kAccountsPrefAllowNewUser,
                                   !policy.has_user_whitelist());
    }
  } else {
    // No configured allow-new-users value, enforce whitelist if non-empty.
    new_values_cache->SetBoolean(
        kAccountsPrefAllowNewUser,
        policy.user_whitelist().user_whitelist_size() == 0);
  }

  new_values_cache->SetBoolean(
      kRebootOnShutdown,
      policy.has_reboot_on_shutdown() &&
      policy.reboot_on_shutdown().has_reboot_on_shutdown() &&
      policy.reboot_on_shutdown().reboot_on_shutdown());

  new_values_cache->SetBoolean(
      kAccountsPrefAllowGuest,
      !policy.has_guest_mode_enabled() ||
      !policy.guest_mode_enabled().has_guest_mode_enabled() ||
      policy.guest_mode_enabled().guest_mode_enabled());

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool supervised_users_enabled = false;
  if (connector->IsEnterpriseManaged()) {
    supervised_users_enabled =
        policy.has_supervised_users_settings() &&
        policy.supervised_users_settings().has_supervised_users_enabled() &&
        policy.supervised_users_settings().supervised_users_enabled();
  } else {
    supervised_users_enabled =
        !policy.has_supervised_users_settings() ||
        !policy.supervised_users_settings().has_supervised_users_enabled() ||
        policy.supervised_users_settings().supervised_users_enabled();
  }
  new_values_cache->SetBoolean(
      kAccountsPrefSupervisedUsersEnabled, supervised_users_enabled);

  new_values_cache->SetBoolean(
      kAccountsPrefShowUserNamesOnSignIn,
      !policy.has_show_user_names() ||
      !policy.show_user_names().has_show_user_names() ||
      policy.show_user_names().show_user_names());

  new_values_cache->SetBoolean(
      kAccountsPrefEphemeralUsersEnabled,
      policy.has_ephemeral_users_enabled() &&
      policy.ephemeral_users_enabled().has_ephemeral_users_enabled() &&
      policy.ephemeral_users_enabled().ephemeral_users_enabled());

  scoped_ptr<base::ListValue> list(new base::ListValue());
  const em::UserWhitelistProto& whitelist_proto = policy.user_whitelist();
  const RepeatedPtrField<std::string>& whitelist =
      whitelist_proto.user_whitelist();
  for (RepeatedPtrField<std::string>::const_iterator it = whitelist.begin();
       it != whitelist.end(); ++it) {
    list->Append(new base::StringValue(*it));
  }
  new_values_cache->SetValue(kAccountsPrefUsers, std::move(list));

  scoped_ptr<base::ListValue> account_list(new base::ListValue());
  const em::DeviceLocalAccountsProto device_local_accounts_proto =
      policy.device_local_accounts();
  const RepeatedPtrField<em::DeviceLocalAccountInfoProto>& accounts =
      device_local_accounts_proto.account();
  RepeatedPtrField<em::DeviceLocalAccountInfoProto>::const_iterator entry;
  for (entry = accounts.begin(); entry != accounts.end(); ++entry) {
    scoped_ptr<base::DictionaryValue> entry_dict(new base::DictionaryValue());
    if (entry->has_type()) {
      if (entry->has_account_id()) {
        entry_dict->SetStringWithoutPathExpansion(
            kAccountsPrefDeviceLocalAccountsKeyId, entry->account_id());
      }
      entry_dict->SetIntegerWithoutPathExpansion(
          kAccountsPrefDeviceLocalAccountsKeyType, entry->type());
      if (entry->kiosk_app().has_app_id()) {
        entry_dict->SetStringWithoutPathExpansion(
            kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
            entry->kiosk_app().app_id());
      }
      if (entry->kiosk_app().has_update_url()) {
        entry_dict->SetStringWithoutPathExpansion(
            kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
            entry->kiosk_app().update_url());
      }
    } else if (entry->has_deprecated_public_session_id()) {
      // Deprecated public session specification.
      entry_dict->SetStringWithoutPathExpansion(
          kAccountsPrefDeviceLocalAccountsKeyId,
          entry->deprecated_public_session_id());
      entry_dict->SetIntegerWithoutPathExpansion(
          kAccountsPrefDeviceLocalAccountsKeyType,
          policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION);
    }
    account_list->Append(std::move(entry_dict));
  }
  new_values_cache->SetValue(kAccountsPrefDeviceLocalAccounts,
                             std::move(account_list));

  if (policy.has_device_local_accounts()) {
    if (policy.device_local_accounts().has_auto_login_id()) {
      new_values_cache->SetString(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          policy.device_local_accounts().auto_login_id());
    }
    if (policy.device_local_accounts().has_auto_login_delay()) {
      new_values_cache->SetInteger(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay,
          policy.device_local_accounts().auto_login_delay());
    }
  }

  new_values_cache->SetBoolean(
      kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
      policy.device_local_accounts().enable_auto_login_bailout());
  new_values_cache->SetBoolean(
      kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline,
      policy.device_local_accounts().prompt_for_network_when_offline());

  if (policy.has_start_up_flags()) {
    scoped_ptr<base::ListValue> list(new base::ListValue());
    const em::StartUpFlagsProto& flags_proto = policy.start_up_flags();
    const RepeatedPtrField<std::string>& flags = flags_proto.flags();
    for (RepeatedPtrField<std::string>::const_iterator it = flags.begin();
         it != flags.end(); ++it) {
      list->Append(new base::StringValue(*it));
    }
    new_values_cache->SetValue(kStartUpFlags, std::move(list));
  }

  if (policy.has_saml_settings()) {
    new_values_cache->SetBoolean(
        kAccountsPrefTransferSAMLCookies,
        policy.saml_settings().transfer_saml_cookies());
  }

  // The behavior when policy is not set and when it is set to an empty string
  // is the same. Thus lets add policy only if it is set and its value is not
  // an empty string.
  if (policy.has_login_screen_domain_auto_complete() &&
      policy.login_screen_domain_auto_complete()
          .has_login_screen_domain_auto_complete() &&
      !policy.login_screen_domain_auto_complete()
           .login_screen_domain_auto_complete()
           .empty()) {
    new_values_cache->SetString(kAccountsPrefLoginScreenDomainAutoComplete,
                                policy.login_screen_domain_auto_complete()
                                    .login_screen_domain_auto_complete());
  }
}

void DecodeNetworkPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) {
  // kSignedDataRoamingEnabled has a default value of false.
  new_values_cache->SetBoolean(
      kSignedDataRoamingEnabled,
      policy.has_data_roaming_enabled() &&
      policy.data_roaming_enabled().has_data_roaming_enabled() &&
      policy.data_roaming_enabled().data_roaming_enabled());
}

void DecodeAutoUpdatePolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) {
  if (policy.has_auto_update_settings()) {
    const em::AutoUpdateSettingsProto& au_settings_proto =
        policy.auto_update_settings();
    if (au_settings_proto.has_update_disabled()) {
      new_values_cache->SetBoolean(kUpdateDisabled,
                                   au_settings_proto.update_disabled());
    }
    const RepeatedField<int>& allowed_connection_types =
        au_settings_proto.allowed_connection_types();
    scoped_ptr<base::ListValue> list(new base::ListValue());
    for (RepeatedField<int>::const_iterator i(allowed_connection_types.begin());
         i != allowed_connection_types.end(); ++i) {
      list->Append(new base::FundamentalValue(*i));
    }
    new_values_cache->SetValue(kAllowedConnectionTypesForUpdate,
                               std::move(list));
  }
}

void DecodeReportingPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) {
  if (policy.has_device_reporting()) {
    const em::DeviceReportingProto& reporting_policy =
        policy.device_reporting();
    if (reporting_policy.has_report_version_info()) {
      new_values_cache->SetBoolean(
          kReportDeviceVersionInfo,
          reporting_policy.report_version_info());
    }
    if (reporting_policy.has_report_activity_times()) {
      new_values_cache->SetBoolean(
          kReportDeviceActivityTimes,
          reporting_policy.report_activity_times());
    }
    if (reporting_policy.has_report_boot_mode()) {
      new_values_cache->SetBoolean(
          kReportDeviceBootMode,
          reporting_policy.report_boot_mode());
    }
    if (reporting_policy.has_report_network_interfaces()) {
      new_values_cache->SetBoolean(
          kReportDeviceNetworkInterfaces,
          reporting_policy.report_network_interfaces());
    }
    if (reporting_policy.has_report_users()) {
      new_values_cache->SetBoolean(
          kReportDeviceUsers,
          reporting_policy.report_users());
    }
    if (reporting_policy.has_report_hardware_status()) {
      new_values_cache->SetBoolean(
          kReportDeviceHardwareStatus,
          reporting_policy.report_hardware_status());
    }
    if (reporting_policy.has_report_session_status()) {
      new_values_cache->SetBoolean(
          kReportDeviceSessionStatus,
          reporting_policy.report_session_status());
    }
    if (reporting_policy.has_device_status_frequency()) {
      new_values_cache->SetInteger(
          kReportUploadFrequency,
          reporting_policy.device_status_frequency());
    }
  }
}

void DecodeHeartbeatPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) {
  if (!policy.has_device_heartbeat_settings())
    return;

  const em::DeviceHeartbeatSettingsProto& heartbeat_policy =
      policy.device_heartbeat_settings();
  if (heartbeat_policy.has_heartbeat_enabled()) {
    new_values_cache->SetBoolean(
        kHeartbeatEnabled,
        heartbeat_policy.heartbeat_enabled());
  }
  if (heartbeat_policy.has_heartbeat_frequency()) {
    new_values_cache->SetInteger(
        kHeartbeatFrequency,
        heartbeat_policy.heartbeat_frequency());
  }
}

void DecodeGenericPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) {
  if (policy.has_metrics_enabled()) {
    new_values_cache->SetBoolean(kStatsReportingPref,
                                 policy.metrics_enabled().metrics_enabled());
  } else {
    new_values_cache->SetBoolean(kStatsReportingPref, false);
  }

  if (!policy.has_release_channel() ||
      !policy.release_channel().has_release_channel()) {
    // Default to an invalid channel (will be ignored).
    new_values_cache->SetString(kReleaseChannel, "");
  } else {
    new_values_cache->SetString(kReleaseChannel,
                                policy.release_channel().release_channel());
  }

  new_values_cache->SetBoolean(
      kReleaseChannelDelegated,
      policy.has_release_channel() &&
      policy.release_channel().has_release_channel_delegated() &&
      policy.release_channel().release_channel_delegated());

  if (policy.has_system_timezone()) {
    if (policy.system_timezone().has_timezone()) {
      new_values_cache->SetString(
          kSystemTimezonePolicy,
          policy.system_timezone().timezone());
    }
  }

  if (policy.has_use_24hour_clock()) {
    if (policy.use_24hour_clock().has_use_24hour_clock()) {
      new_values_cache->SetBoolean(
          kSystemUse24HourClock, policy.use_24hour_clock().use_24hour_clock());
    }
  }

  if (policy.has_allow_redeem_offers()) {
    new_values_cache->SetBoolean(
        kAllowRedeemChromeOsRegistrationOffers,
        policy.allow_redeem_offers().allow_redeem_offers());
  } else {
    new_values_cache->SetBoolean(
        kAllowRedeemChromeOsRegistrationOffers,
        true);
  }

  if (policy.has_variations_parameter()) {
    new_values_cache->SetString(
        kVariationsRestrictParameter,
        policy.variations_parameter().parameter());
  }

  new_values_cache->SetBoolean(
      kDeviceAttestationEnabled,
      policy.attestation_settings().attestation_enabled());

  if (policy.has_attestation_settings() &&
      policy.attestation_settings().has_content_protection_enabled()) {
    new_values_cache->SetBoolean(
        kAttestationForContentProtectionEnabled,
        policy.attestation_settings().content_protection_enabled());
  } else {
    new_values_cache->SetBoolean(kAttestationForContentProtectionEnabled, true);
  }

  if (policy.has_extension_cache_size() &&
      policy.extension_cache_size().has_extension_cache_size()) {
    new_values_cache->SetInteger(
        kExtensionCacheSize,
        policy.extension_cache_size().extension_cache_size());
  }

  if (policy.has_display_rotation_default() &&
      policy.display_rotation_default().has_display_rotation_default()) {
    new_values_cache->SetInteger(
        kDisplayRotationDefault,
        policy.display_rotation_default().display_rotation_default());
  }
}

void DecodeLogUploadPolicies(const em::ChromeDeviceSettingsProto& policy,
                             PrefValueMap* new_values_cache) {
  if (!policy.has_device_log_upload_settings())
    return;

  const em::DeviceLogUploadSettingsProto& log_upload_policy =
      policy.device_log_upload_settings();
  if (log_upload_policy.has_system_log_upload_enabled()) {
    new_values_cache->SetBoolean(kSystemLogUploadEnabled,
                                 log_upload_policy.system_log_upload_enabled());
  }
}

void DecodeDeviceState(const em::PolicyData& policy_data,
                       PrefValueMap* new_values_cache) {
  if (!policy_data.has_device_state())
    return;

  const em::DeviceState& device_state = policy_data.device_state();

  if (device_state.device_mode() == em::DeviceState::DEVICE_MODE_DISABLED)
    new_values_cache->SetBoolean(kDeviceDisabled, true);
  if (device_state.has_disabled_state() &&
      device_state.disabled_state().has_message()) {
    new_values_cache->SetString(kDeviceDisabledMessage,
                                device_state.disabled_state().message());
  }
}

}  // namespace

DeviceSettingsProvider::DeviceSettingsProvider(
    const NotifyObserversCallback& notify_cb,
    DeviceSettingsService* device_settings_service)
    : CrosSettingsProvider(notify_cb),
      device_settings_service_(device_settings_service),
      trusted_status_(TEMPORARILY_UNTRUSTED),
      ownership_status_(device_settings_service_->GetOwnershipStatus()),
      store_callback_factory_(this) {
  device_settings_service_->AddObserver(this);
  if (!UpdateFromService()) {
    // Make sure we have at least the cache data immediately.
    RetrieveCachedData();
  }
}

DeviceSettingsProvider::~DeviceSettingsProvider() {
  if (device_settings_service_->GetOwnerSettingsService())
    device_settings_service_->GetOwnerSettingsService()->RemoveObserver(this);
  device_settings_service_->RemoveObserver(this);
}

// static
bool DeviceSettingsProvider::IsDeviceSetting(const std::string& name) {
  const char* const* end = kKnownSettings + arraysize(kKnownSettings);
  return std::find(kKnownSettings, end, name) != end;
}

void DeviceSettingsProvider::DoSet(const std::string& path,
                                   const base::Value& in_value) {
  // Make sure that either the current user is the device owner or the
  // device doesn't have an owner yet.
  if (!(device_settings_service_->HasPrivateOwnerKey() ||
        ownership_status_ == DeviceSettingsService::OWNERSHIP_NONE)) {
    LOG(WARNING) << "Changing settings from non-owner, setting=" << path;

    // Revert UI change.
    NotifyObservers(path);
    return;
  }

  if (!IsDeviceSetting(path)) {
    NOTREACHED() << "Try to set unhandled cros setting " << path;
    return;
  }

  if (device_settings_service_->HasPrivateOwnerKey()) {
    // Directly set setting through OwnerSettingsService.
    ownership::OwnerSettingsService* service =
        device_settings_service_->GetOwnerSettingsService();
    if (!service->Set(path, in_value)) {
      NotifyObservers(path);
      return;
    }
  } else {
    // Temporary store new setting in
    // |device_settings_|. |device_settings_| will be stored on a disk
    // as soon as an ownership of device the will be taken.
    OwnerSettingsServiceChromeOS::UpdateDeviceSettings(
        path, in_value, device_settings_);
    em::PolicyData data;
    data.set_username(device_settings_service_->GetUsername());
    CHECK(device_settings_.SerializeToString(data.mutable_policy_value()));

    // Set the cache to the updated value.
    UpdateValuesCache(data, device_settings_, TEMPORARILY_UNTRUSTED);

    if (!device_settings_cache::Store(data, g_browser_process->local_state())) {
      LOG(ERROR) << "Couldn't store to the temp storage.";
      NotifyObservers(path);
      return;
    }
  }

}

void DeviceSettingsProvider::OwnershipStatusChanged() {
  DeviceSettingsService::OwnershipStatus new_ownership_status =
      device_settings_service_->GetOwnershipStatus();

  if (device_settings_service_->GetOwnerSettingsService())
    device_settings_service_->GetOwnerSettingsService()->AddObserver(this);

  // If the device just became owned, write the settings accumulated in the
  // cache to device settings proper. It is important that writing only happens
  // in this case, as during normal operation, the contents of the cache should
  // never overwrite actual device settings.
  if (new_ownership_status == DeviceSettingsService::OWNERSHIP_TAKEN &&
      ownership_status_ == DeviceSettingsService::OWNERSHIP_NONE &&
      device_settings_service_->HasPrivateOwnerKey()) {
    // There shouldn't be any pending writes, since the cache writes are all
    // immediate.
    DCHECK(!store_callback_factory_.HasWeakPtrs());

    trusted_status_ = TEMPORARILY_UNTRUSTED;
    // Apply the locally-accumulated device settings on top of the initial
    // settings from the service and write back the result.
    if (device_settings_service_->device_settings()) {
      em::ChromeDeviceSettingsProto new_settings(
          *device_settings_service_->device_settings());
      new_settings.MergeFrom(device_settings_);
      device_settings_.Swap(&new_settings);
    }

    scoped_ptr<em::PolicyData> policy(new em::PolicyData());
    policy->set_username(device_settings_service_->GetUsername());
    CHECK(device_settings_.SerializeToString(policy->mutable_policy_value()));
    if (!device_settings_service_->GetOwnerSettingsService()
             ->CommitTentativeDeviceSettings(std::move(policy))) {
      LOG(ERROR) << "Can't store policy";
    }
  }

  ownership_status_ = new_ownership_status;
}

void DeviceSettingsProvider::DeviceSettingsUpdated() {
  if (!store_callback_factory_.HasWeakPtrs())
    UpdateAndProceedStoring();
}

void DeviceSettingsProvider::OnDeviceSettingsServiceShutdown() {
  device_settings_service_ = nullptr;
}

void DeviceSettingsProvider::OnTentativeChangesInPolicy(
    const em::PolicyData& policy_data) {
  em::ChromeDeviceSettingsProto device_settings;
  CHECK(device_settings.ParseFromString(policy_data.policy_value()));
  UpdateValuesCache(policy_data, device_settings, TEMPORARILY_UNTRUSTED);
}

void DeviceSettingsProvider::RetrieveCachedData() {
  em::PolicyData policy_data;
  if (!device_settings_cache::Retrieve(&policy_data,
                                       g_browser_process->local_state()) ||
      !device_settings_.ParseFromString(policy_data.policy_value())) {
    VLOG(1) << "Can't retrieve temp store, possibly not created yet.";
  }

  UpdateValuesCache(policy_data, device_settings_, trusted_status_);
}

void DeviceSettingsProvider::UpdateValuesCache(
    const em::PolicyData& policy_data,
    const em::ChromeDeviceSettingsProto& settings,
    TrustedStatus trusted_status) {
  PrefValueMap new_values_cache;

  // If the device is not managed, or is consumer-managed, we set the device
  // owner value.
  if (policy_data.has_username() &&
      (policy::GetManagementMode(policy_data) ==
           policy::MANAGEMENT_MODE_LOCAL_OWNER ||
       policy::GetManagementMode(policy_data) ==
           policy::MANAGEMENT_MODE_CONSUMER_MANAGED)) {
    new_values_cache.SetString(kDeviceOwner, policy_data.username());
  }

  if (policy_data.has_service_account_identity()) {
    new_values_cache.SetString(kServiceAccountIdentity,
                               policy_data.service_account_identity());
  }

  DecodeLoginPolicies(settings, &new_values_cache);
  DecodeNetworkPolicies(settings, &new_values_cache);
  DecodeAutoUpdatePolicies(settings, &new_values_cache);
  DecodeReportingPolicies(settings, &new_values_cache);
  DecodeHeartbeatPolicies(settings, &new_values_cache);
  DecodeGenericPolicies(settings, &new_values_cache);
  DecodeLogUploadPolicies(settings, &new_values_cache);
  DecodeDeviceState(policy_data, &new_values_cache);

  // Collect all notifications but send them only after we have swapped the
  // cache so that if somebody actually reads the cache will be already valid.
  std::vector<std::string> notifications;
  // Go through the new values and verify in the old ones.
  PrefValueMap::iterator iter = new_values_cache.begin();
  for (; iter != new_values_cache.end(); ++iter) {
    const base::Value* old_value;
    if (!values_cache_.GetValue(iter->first, &old_value) ||
        !old_value->Equals(iter->second)) {
      notifications.push_back(iter->first);
    }
  }
  // Now check for values that have been removed from the policy blob.
  for (iter = values_cache_.begin(); iter != values_cache_.end(); ++iter) {
    const base::Value* value;
    if (!new_values_cache.GetValue(iter->first, &value))
      notifications.push_back(iter->first);
  }
  // Swap and notify.
  values_cache_.Swap(&new_values_cache);
  trusted_status_ = trusted_status;
  for (size_t i = 0; i < notifications.size(); ++i)
    NotifyObservers(notifications[i]);
}

bool DeviceSettingsProvider::MitigateMissingPolicy() {
  // First check if the device has been owned already and if not exit
  // immediately.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->GetDeviceMode() != policy::DEVICE_MODE_CONSUMER)
    return false;

  // If we are here the policy file were corrupted or missing. This can happen
  // because we are migrating Pre R11 device to the new secure policies or there
  // was an attempt to circumvent policy system. In this case we should populate
  // the policy cache with "safe-mode" defaults which should allow the owner to
  // log in but lock the device for anyone else until the policy blob has been
  // recreated by the session manager.
  LOG(ERROR) << "Corruption of the policy data has been detected."
             << "Switching to \"safe-mode\" policies until the owner logs in "
             << "to regenerate the policy data.";

  device_settings_.Clear();
  device_settings_.mutable_allow_new_users()->set_allow_new_users(true);
  device_settings_.mutable_guest_mode_enabled()->set_guest_mode_enabled(true);
  em::PolicyData empty_policy_data;
  UpdateValuesCache(empty_policy_data, device_settings_, TRUSTED);
  values_cache_.SetBoolean(kPolicyMissingMitigationMode, true);

  return true;
}

const base::Value* DeviceSettingsProvider::Get(const std::string& path) const {
  if (IsDeviceSetting(path)) {
    const base::Value* value;
    if (values_cache_.GetValue(path, &value))
      return value;
  } else {
    NOTREACHED() << "Trying to get non cros setting.";
  }

  return NULL;
}

DeviceSettingsProvider::TrustedStatus
    DeviceSettingsProvider::PrepareTrustedValues(const base::Closure& cb) {
  TrustedStatus status = RequestTrustedEntity();
  if (status == TEMPORARILY_UNTRUSTED && !cb.is_null())
    callbacks_.push_back(cb);
  return status;
}

bool DeviceSettingsProvider::HandlesSetting(const std::string& path) const {
  return IsDeviceSetting(path);
}

DeviceSettingsProvider::TrustedStatus
    DeviceSettingsProvider::RequestTrustedEntity() {
  if (ownership_status_ == DeviceSettingsService::OWNERSHIP_NONE)
    return TRUSTED;
  return trusted_status_;
}

void DeviceSettingsProvider::UpdateAndProceedStoring() {
  // Re-sync the cache from the service.
  UpdateFromService();
}

bool DeviceSettingsProvider::UpdateFromService() {
  bool settings_loaded = false;
  switch (device_settings_service_->status()) {
    case DeviceSettingsService::STORE_SUCCESS: {
      const em::PolicyData* policy_data =
          device_settings_service_->policy_data();
      const em::ChromeDeviceSettingsProto* device_settings =
          device_settings_service_->device_settings();
      if (policy_data && device_settings) {
        if (!device_settings_cache::Store(*policy_data,
                                          g_browser_process->local_state())) {
          LOG(ERROR) << "Couldn't update the local state cache.";
        }
        UpdateValuesCache(*policy_data, *device_settings, TRUSTED);
        device_settings_ = *device_settings;

        settings_loaded = true;
      } else {
        // Initial policy load is still pending.
        trusted_status_ = TEMPORARILY_UNTRUSTED;
      }
      break;
    }
    case DeviceSettingsService::STORE_NO_POLICY:
      if (MitigateMissingPolicy())
        break;
      // fall through.
    case DeviceSettingsService::STORE_KEY_UNAVAILABLE:
      VLOG(1) << "No policies present yet, will use the temp storage.";
      trusted_status_ = PERMANENTLY_UNTRUSTED;
      break;
    case DeviceSettingsService::STORE_POLICY_ERROR:
    case DeviceSettingsService::STORE_VALIDATION_ERROR:
    case DeviceSettingsService::STORE_INVALID_POLICY:
    case DeviceSettingsService::STORE_OPERATION_FAILED:
      LOG(ERROR) << "Failed to retrieve cros policies. Reason: "
                 << device_settings_service_->status();
      trusted_status_ = PERMANENTLY_UNTRUSTED;
      break;
    case DeviceSettingsService::STORE_TEMP_VALIDATION_ERROR:
      // The policy has failed to validate due to temporary error but it might
      // take a long time until we recover so behave as it is a permanent error.
      LOG(ERROR) << "Failed to retrieve cros policies because a temporary "
                 << "validation error has occurred. Retrying might succeed.";
      trusted_status_ = PERMANENTLY_UNTRUSTED;
      break;
  }

  // Notify the observers we are done.
  std::vector<base::Closure> callbacks;
  callbacks.swap(callbacks_);
  for (size_t i = 0; i < callbacks.size(); ++i)
    callbacks[i].Run();

  return settings_loaded;
}

}  // namespace chromeos
