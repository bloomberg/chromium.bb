// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_decoder_chromeos.h"

#include <limits>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/app_pack_updater.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/policy/policy_map.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#include "policy/policy_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

namespace em = enterprise_management;

namespace policy {

namespace {

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

Value* DecodeConnectionType(int value) {
  static const char* const kConnectionTypes[] = {
    flimflam::kTypeEthernet,
    flimflam::kTypeWifi,
    flimflam::kTypeWimax,
    flimflam::kTypeBluetooth,
    flimflam::kTypeCellular,
  };

  if (value < 0 || value >= static_cast<int>(arraysize(kConnectionTypes)))
    return NULL;

  return Value::CreateStringValue(kConnectionTypes[value]);
}

void DecodeLoginPolicies(const em::ChromeDeviceSettingsProto& policy,
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

  if (policy.has_device_local_accounts()) {
    const em::DeviceLocalAccountsProto& container(
        policy.device_local_accounts());
    const RepeatedPtrField<em::DeviceLocalAccountInfoProto>& accounts =
        container.account();
    if (accounts.size() > 0) {
      ListValue* account_list = new ListValue();
      RepeatedPtrField<em::DeviceLocalAccountInfoProto>::const_iterator entry;
      for (entry = accounts.begin(); entry != accounts.end(); ++entry) {
        if (entry->has_id())
          account_list->AppendString(entry->id());
      }
      policies->Set(key::kDeviceLocalAccounts,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    account_list);
    }
    if (container.has_auto_login_id()) {
      policies->Set(key::kDeviceLocalAccountAutoLoginId,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateStringValue(container.auto_login_id()));
    }
    if (container.has_auto_login_delay()) {
      policies->Set(key::kDeviceLocalAccountAutoLoginDelay,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    DecodeIntegerValue(container.auto_login_delay()));
    }
  }
}

void DecodeKioskPolicies(const em::ChromeDeviceSettingsProto& policy,
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

void DecodeNetworkPolicies(const em::ChromeDeviceSettingsProto& policy,
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

void DecodeReportingPolicies(const em::ChromeDeviceSettingsProto& policy,
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

void DecodeAutoUpdatePolicies(const em::ChromeDeviceSettingsProto& policy,
                              PolicyMap* policies) {
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

    // target_version_display_name is not actually a policy, but a display
    // string for target_version_prefix, so we ignore it.

    if (container.has_scatter_factor_in_seconds()) {
      policies->Set(key::kDeviceUpdateScatterFactor,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateIntegerValue(
                        container.scatter_factor_in_seconds()));
    }

    if (container.allowed_connection_types_size()) {
      ListValue* allowed_connection_types = new ListValue();
      RepeatedField<int>::const_iterator entry;
      for (entry = container.allowed_connection_types().begin();
           entry != container.allowed_connection_types().end();
           ++entry) {
        base::Value* value = DecodeConnectionType(*entry);
        if (value)
          allowed_connection_types->Append(value);
      }
      policies->Set(key::kDeviceUpdateAllowedConnectionTypes,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    allowed_connection_types);
    }

    if (container.has_reboot_after_update()) {
      policies->Set(key::kRebootAfterUpdate,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(container.reboot_after_update()));
    }
  }
}

void DecodeGenericPolicies(const em::ChromeDeviceSettingsProto& policy,
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

  if (policy.has_system_timezone()) {
    if (policy.system_timezone().has_timezone()) {
      policies->Set(key::kSystemTimezone,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateStringValue(
                        policy.system_timezone().timezone()));
    }
  }

  if (policy.has_allow_redeem_offers()) {
    const em::AllowRedeemChromeOsRegistrationOffersProto& container(
        policy.allow_redeem_offers());
    if (container.has_allow_redeem_offers()) {
      policies->Set(key::kDeviceAllowRedeemChromeOsRegistrationOffers,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    Value::CreateBooleanValue(
                        container.allow_redeem_offers()));
    }
  }

  if (policy.has_uptime_limit()) {
    const em::UptimeLimitProto& container(policy.uptime_limit());
    if (container.has_uptime_limit()) {
      policies->Set(key::kUptimeLimit,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    DecodeIntegerValue(container.uptime_limit()));
    }
  }

  if (policy.has_start_up_flags()) {
    const em::StartUpFlagsProto& container(policy.start_up_flags());
    if (container.flags_size()) {
      ListValue* flags = new ListValue();
      RepeatedPtrField<std::string>::const_iterator entry;
      for (entry = container.flags().begin();
           entry != container.flags().end();
           ++entry) {
        flags->Append(Value::CreateStringValue(*entry));
      }
      policies->Set(key::kDeviceStartUpFlags,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    flags);
    }
  }
}

}  // namespace

void DecodeDevicePolicy(const em::ChromeDeviceSettingsProto& policy,
                        PolicyMap* policies,
                        EnterpriseInstallAttributes* install_attributes) {
  // Decode the various groups of policies.
  DecodeLoginPolicies(policy, policies);
  DecodeKioskPolicies(policy, policies, install_attributes);
  DecodeNetworkPolicies(policy, policies, install_attributes);
  DecodeReportingPolicies(policy, policies);
  DecodeAutoUpdatePolicies(policy, policies);
  DecodeGenericPolicies(policy, policies);
}

}  // namespace policy
