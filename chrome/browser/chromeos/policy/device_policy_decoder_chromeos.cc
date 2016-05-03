// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_decoder_chromeos.h"

#include <limits>
#include <string>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "policy/policy_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

namespace em = enterprise_management;

namespace policy {

namespace {

// Decodes a protobuf integer to an IntegerValue. Returns NULL in case the input
// value is out of bounds.
std::unique_ptr<base::Value> DecodeIntegerValue(google::protobuf::int64 value) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return std::unique_ptr<base::Value>();
  }

  return std::unique_ptr<base::Value>(
      new base::FundamentalValue(static_cast<int>(value)));
}

// Decodes a JSON string to a base::Value, and drops unknown properties
// according to a policy schema. |policy_name| is the name of a policy schema
// defined in policy_templates.json. Returns NULL in case the input is not a
// valid JSON string.
std::unique_ptr<base::Value> DecodeJsonStringAndDropUnknownBySchema(
    const std::string& json_string,
    const std::string& policy_name) {
  std::string error;
  std::unique_ptr<base::Value> root = base::JSONReader::ReadAndReturnError(
      json_string, base::JSON_ALLOW_TRAILING_COMMAS, NULL, &error);

  if (!root) {
    LOG(WARNING) << "Invalid JSON string: " << error << ", ignoring.";
    return std::unique_ptr<base::Value>();
  }

  const Schema& schema = g_browser_process
                             ->browser_policy_connector()
                             ->GetChromeSchema()
                             .GetKnownProperty(policy_name);

  if (schema.valid()) {
    std::string error_path;
    bool changed = false;

    if (!schema.Normalize(root.get(), SCHEMA_ALLOW_UNKNOWN, &error_path, &error,
                          &changed)) {
      LOG(WARNING) << "Invalid policy value for " << policy_name << ": "
                   << error << " at " << error_path << ".";
      return std::unique_ptr<base::Value>();
    }

    if (changed) {
      LOG(WARNING) << "Some properties in " << policy_name
                   << " were dropped: " << error << " at " << error_path << ".";
    }
  } else {
    LOG(WARNING) << "Unknown or invalid policy schema for " << policy_name
                 << ".";
    return std::unique_ptr<base::Value>();
  }

  return root;
}

base::Value* DecodeConnectionType(int value) {
  static const char* const kConnectionTypes[] = {
    shill::kTypeEthernet,
    shill::kTypeWifi,
    shill::kTypeWimax,
    shill::kTypeBluetooth,
    shill::kTypeCellular,
  };

  if (value < 0 || value >= static_cast<int>(arraysize(kConnectionTypes)))
    return NULL;

  return new base::StringValue(kConnectionTypes[value]);
}

void DecodeLoginPolicies(const em::ChromeDeviceSettingsProto& policy,
                         PolicyMap* policies) {
  if (policy.has_guest_mode_enabled()) {
    const em::GuestModeEnabledProto& container(policy.guest_mode_enabled());
    if (container.has_guest_mode_enabled()) {
      policies->Set(key::kDeviceGuestModeEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.guest_mode_enabled()),
                    NULL);
    }
  }

  if (policy.has_reboot_on_shutdown()) {
    const em::RebootOnShutdownProto& container(policy.reboot_on_shutdown());
    if (container.has_reboot_on_shutdown()) {
      policies->Set(key::kDeviceRebootOnShutdown, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(container.reboot_on_shutdown()),
                    NULL);
    }
  }

  if (policy.has_show_user_names()) {
    const em::ShowUserNamesOnSigninProto& container(policy.show_user_names());
    if (container.has_show_user_names()) {
      policies->Set(key::kDeviceShowUserNamesOnSignin,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.show_user_names()),
                    NULL);
    }
  }

  if (policy.has_allow_new_users()) {
    const em::AllowNewUsersProto& container(policy.allow_new_users());
    if (container.has_allow_new_users()) {
      policies->Set(key::kDeviceAllowNewUsers,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.allow_new_users()),
                    NULL);
    }
  }

  if (policy.has_user_whitelist()) {
    const em::UserWhitelistProto& container(policy.user_whitelist());
    base::ListValue* whitelist = new base::ListValue();
    RepeatedPtrField<std::string>::const_iterator entry;
    for (entry = container.user_whitelist().begin();
         entry != container.user_whitelist().end();
         ++entry) {
      whitelist->Append(new base::StringValue(*entry));
    }
    policies->Set(key::kDeviceUserWhitelist,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD,
                  whitelist,
                  NULL);
  }

  if (policy.has_ephemeral_users_enabled()) {
    const em::EphemeralUsersEnabledProto& container(
        policy.ephemeral_users_enabled());
    if (container.has_ephemeral_users_enabled()) {
      policies->Set(key::kDeviceEphemeralUsersEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.ephemeral_users_enabled()),
                    NULL);
    }
  }

  if (policy.has_device_local_accounts()) {
    const em::DeviceLocalAccountsProto& container(
        policy.device_local_accounts());
    const RepeatedPtrField<em::DeviceLocalAccountInfoProto>& accounts =
        container.account();
    std::unique_ptr<base::ListValue> account_list(new base::ListValue());
    RepeatedPtrField<em::DeviceLocalAccountInfoProto>::const_iterator entry;
    for (entry = accounts.begin(); entry != accounts.end(); ++entry) {
      std::unique_ptr<base::DictionaryValue> entry_dict(
          new base::DictionaryValue());
      if (entry->has_type()) {
        if (entry->has_account_id()) {
          entry_dict->SetStringWithoutPathExpansion(
              chromeos::kAccountsPrefDeviceLocalAccountsKeyId,
              entry->account_id());
        }
        entry_dict->SetIntegerWithoutPathExpansion(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyType, entry->type());
        if (entry->kiosk_app().has_app_id()) {
          entry_dict->SetStringWithoutPathExpansion(
              chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
              entry->kiosk_app().app_id());
        }
        if (entry->kiosk_app().has_update_url()) {
          entry_dict->SetStringWithoutPathExpansion(
              chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
              entry->kiosk_app().update_url());
        }
      } else if (entry->has_deprecated_public_session_id()) {
        // Deprecated public session specification.
        entry_dict->SetStringWithoutPathExpansion(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyId,
            entry->deprecated_public_session_id());
        entry_dict->SetIntegerWithoutPathExpansion(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyType,
            DeviceLocalAccount::TYPE_PUBLIC_SESSION);
      }
      account_list->Append(entry_dict.release());
    }
    policies->Set(key::kDeviceLocalAccounts,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD,
                  account_list.release(),
                  NULL);
    if (container.has_auto_login_id()) {
      policies->Set(key::kDeviceLocalAccountAutoLoginId,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::StringValue(container.auto_login_id()),
                    NULL);
    }
    if (container.has_auto_login_delay()) {
      policies->Set(key::kDeviceLocalAccountAutoLoginDelay,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(container.auto_login_delay()).release(),
                    NULL);
    }
    if (container.has_enable_auto_login_bailout()) {
      policies->Set(key::kDeviceLocalAccountAutoLoginBailoutEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.enable_auto_login_bailout()),
                    NULL);
    }
    if (container.has_prompt_for_network_when_offline()) {
      policies->Set(key::kDeviceLocalAccountPromptForNetworkWhenOffline,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.prompt_for_network_when_offline()),
                    NULL);
    }
  }

  if (policy.has_supervised_users_settings()) {
    const em::SupervisedUsersSettingsProto& container =
        policy.supervised_users_settings();
    if (container.has_supervised_users_enabled()) {
      base::Value* value = new base::FundamentalValue(
          container.supervised_users_enabled());
      policies->Set(key::kSupervisedUsersEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    value,
                    NULL);
    }
  }

  if (policy.has_saml_settings()) {
    const em::SAMLSettingsProto& container(policy.saml_settings());
    if (container.has_transfer_saml_cookies()) {
      policies->Set(key::kDeviceTransferSAMLCookies,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.transfer_saml_cookies()),
                    NULL);
    }
  }

  if (policy.has_login_authentication_behavior()) {
    const em::LoginAuthenticationBehaviorProto& container(
        policy.login_authentication_behavior());
    if (container.has_login_authentication_behavior()) {
      policies->Set(key::kLoginAuthenticationBehavior,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(
                        container.login_authentication_behavior()).release(),
                    nullptr);
    }
  }

  if (policy.has_allow_bluetooth()) {
    const em::AllowBluetoothProto& container(policy.allow_bluetooth());
    if (container.has_allow_bluetooth()) {
      policies->Set(key::kDeviceAllowBluetooth, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(container.allow_bluetooth()),
                    nullptr);
    }
  }

  if (policy.has_login_video_capture_allowed_urls()) {
    const em::LoginVideoCaptureAllowedUrlsProto& container(
        policy.login_video_capture_allowed_urls());
    std::unique_ptr<base::ListValue> urls(new base::ListValue());
    for (const auto& entry : container.urls()) {
      urls->Append(new base::StringValue(entry));
    }
    policies->Set(key::kLoginVideoCaptureAllowedUrls, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, urls.release(),
                  nullptr);
  }
}

void DecodeNetworkPolicies(const em::ChromeDeviceSettingsProto& policy,
                           PolicyMap* policies) {
  if (policy.has_data_roaming_enabled()) {
    const em::DataRoamingEnabledProto& container(policy.data_roaming_enabled());
    if (container.has_data_roaming_enabled()) {
      policies->Set(key::kDeviceDataRoamingEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.data_roaming_enabled()),
                    NULL);
    }
  }

  if (policy.has_open_network_configuration() &&
      policy.open_network_configuration().has_open_network_configuration()) {
    std::string config(
        policy.open_network_configuration().open_network_configuration());
    policies->Set(key::kDeviceOpenNetworkConfiguration,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD,
                  new base::StringValue(config),
                  NULL);
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
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.report_version_info()),
                    NULL);
    }
    if (container.has_report_activity_times()) {
      policies->Set(key::kReportDeviceActivityTimes,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.report_activity_times()),
                    NULL);
    }
    if (container.has_report_boot_mode()) {
      policies->Set(key::kReportDeviceBootMode,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.report_boot_mode()),
                    NULL);
    }
    if (container.has_report_location()) {
      policies->Set(key::kReportDeviceLocation,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.report_location()),
                    NULL);
    }
    if (container.has_report_network_interfaces()) {
      policies->Set(key::kReportDeviceNetworkInterfaces,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.report_network_interfaces()),
                    NULL);
    }
    if (container.has_report_users()) {
      policies->Set(key::kReportDeviceUsers,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(container.report_users()),
                    NULL);
    }
    if (container.has_report_hardware_status()) {
      policies->Set(key::kReportDeviceHardwareStatus,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.report_hardware_status()),
                    NULL);
    }
    if (container.has_report_session_status()) {
      policies->Set(key::kReportDeviceSessionStatus,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.report_session_status()),
                    NULL);
    }
    if (container.has_device_status_frequency()) {
      policies->Set(key::kReportUploadFrequency,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(
                        container.device_status_frequency()).release(),
                    NULL);
    }
  }

  if (policy.has_device_heartbeat_settings()) {
    const em::DeviceHeartbeatSettingsProto& container(
        policy.device_heartbeat_settings());
    if (container.has_heartbeat_enabled()) {
      policies->Set(key::kHeartbeatEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.heartbeat_enabled()),
                    NULL);
    }
    if (container.has_heartbeat_frequency()) {
      policies->Set(key::kHeartbeatFrequency,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(
                        container.heartbeat_frequency()).release(),
                    NULL);
    }
  }

  if (policy.has_device_log_upload_settings()) {
    const em::DeviceLogUploadSettingsProto& container(
        policy.device_log_upload_settings());
    if (container.has_system_log_upload_enabled()) {
      policies->Set(
          key::kLogUploadEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          new base::FundamentalValue(container.system_log_upload_enabled()),
          NULL);
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
                    POLICY_SOURCE_CLOUD,
                    new base::StringValue(channel),
                    NULL);
      // TODO(dubroy): Once http://crosbug.com/17015 is implemented, we won't
      // have to pass the channel in here, only ping the update engine to tell
      // it to fetch the channel from the policy.
      chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->
          SetChannel(channel, false);
    }
    if (container.has_release_channel_delegated()) {
      policies->Set(key::kChromeOsReleaseChannelDelegated,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.release_channel_delegated()),
                    NULL);
    }
  }

  if (policy.has_auto_update_settings()) {
    const em::AutoUpdateSettingsProto& container(policy.auto_update_settings());
    if (container.has_update_disabled()) {
      policies->Set(key::kDeviceAutoUpdateDisabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.update_disabled()),
                    NULL);
    }

    if (container.has_target_version_prefix()) {
      policies->Set(key::kDeviceTargetVersionPrefix,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::StringValue(
                        container.target_version_prefix()),
                    NULL);
    }

    // target_version_display_name is not actually a policy, but a display
    // string for target_version_prefix, so we ignore it.

    if (container.has_scatter_factor_in_seconds()) {
      policies->Set(key::kDeviceUpdateScatterFactor,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(static_cast<int>(
                        container.scatter_factor_in_seconds())),
                    NULL);
    }

    if (container.allowed_connection_types_size()) {
      base::ListValue* allowed_connection_types = new base::ListValue();
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
                    POLICY_SOURCE_CLOUD,
                    allowed_connection_types,
                    NULL);
    }

    if (container.has_http_downloads_enabled()) {
      policies->Set(
          key::kDeviceUpdateHttpDownloadsEnabled,
          POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          new base::FundamentalValue(container.http_downloads_enabled()),
          NULL);
    }

    if (container.has_reboot_after_update()) {
      policies->Set(key::kRebootAfterUpdate,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.reboot_after_update()),
                    NULL);
    }

    if (container.has_p2p_enabled()) {
      policies->Set(key::kDeviceAutoUpdateP2PEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(container.p2p_enabled()),
                    NULL);
    }
  }

  if (policy.has_allow_kiosk_app_control_chrome_version()) {
    const em::AllowKioskAppControlChromeVersionProto& container(
        policy.allow_kiosk_app_control_chrome_version());
    if (container.has_allow_kiosk_app_control_chrome_version()) {
      policies->Set(key::kAllowKioskAppControlChromeVersion,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.allow_kiosk_app_control_chrome_version()),
                    NULL);
    }
  }
}

void DecodeAccessibilityPolicies(const em::ChromeDeviceSettingsProto& policy,
                                 PolicyMap* policies) {
  if (policy.has_accessibility_settings()) {
    const em::AccessibilitySettingsProto&
        container(policy.accessibility_settings());

    if (container.has_login_screen_default_large_cursor_enabled()) {
      policies->Set(
          key::kDeviceLoginScreenDefaultLargeCursorEnabled,
          POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          new base::FundamentalValue(
              container.login_screen_default_large_cursor_enabled()),
          NULL);
    }

    if (container.has_login_screen_default_spoken_feedback_enabled()) {
      policies->Set(
          key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
          POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          new base::FundamentalValue(
              container.login_screen_default_spoken_feedback_enabled()),
          NULL);
    }

    if (container.has_login_screen_default_high_contrast_enabled()) {
      policies->Set(
          key::kDeviceLoginScreenDefaultHighContrastEnabled,
          POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          new base::FundamentalValue(
              container.login_screen_default_high_contrast_enabled()),
          NULL);
    }

    if (container.has_login_screen_default_screen_magnifier_type()) {
      policies->Set(
          key::kDeviceLoginScreenDefaultScreenMagnifierType,
          POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          DecodeIntegerValue(
              container.login_screen_default_screen_magnifier_type()).release(),
          NULL);
    }

    if (container.has_login_screen_default_virtual_keyboard_enabled()) {
      policies->Set(
          key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled,
          POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          new base::FundamentalValue(
              container.login_screen_default_virtual_keyboard_enabled()),
          NULL);
    }
  }
}

void DecodeGenericPolicies(const em::ChromeDeviceSettingsProto& policy,
                           PolicyMap* policies) {
  if (policy.has_device_policy_refresh_rate()) {
    const em::DevicePolicyRefreshRateProto& container(
        policy.device_policy_refresh_rate());
    if (container.has_device_policy_refresh_rate()) {
      policies->Set(
          key::kDevicePolicyRefreshRate,
          POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          DecodeIntegerValue(container.device_policy_refresh_rate()).release(),
          NULL);
    }
  }

  if (policy.has_metrics_enabled()) {
    const em::MetricsEnabledProto& container(policy.metrics_enabled());
    if (container.has_metrics_enabled()) {
      policies->Set(key::kDeviceMetricsReportingEnabled,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.metrics_enabled()),
                    NULL);
    }
  }

  if (policy.has_system_timezone()) {
    if (policy.system_timezone().has_timezone()) {
      policies->Set(key::kSystemTimezone,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::StringValue(
                        policy.system_timezone().timezone()),
                    NULL);
    }

    if (policy.system_timezone().has_timezone_detection_type()) {
      std::unique_ptr<base::Value> value(DecodeIntegerValue(
          policy.system_timezone().timezone_detection_type()));
      if (value) {
        policies->Set(key::kSystemTimezoneAutomaticDetection,
                      POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD,
                      value.release(),
                      nullptr);
      }
    }
  }

  if (policy.has_use_24hour_clock()) {
    if (policy.use_24hour_clock().has_use_24hour_clock()) {
      policies->Set(key::kSystemUse24HourClock,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        policy.use_24hour_clock().use_24hour_clock()),
                    NULL);
    }
  }

  if (policy.has_allow_redeem_offers()) {
    const em::AllowRedeemChromeOsRegistrationOffersProto& container(
        policy.allow_redeem_offers());
    if (container.has_allow_redeem_offers()) {
      policies->Set(key::kDeviceAllowRedeemChromeOsRegistrationOffers,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        container.allow_redeem_offers()),
                    NULL);
    }
  }

  if (policy.has_uptime_limit()) {
    const em::UptimeLimitProto& container(policy.uptime_limit());
    if (container.has_uptime_limit()) {
      policies->Set(key::kUptimeLimit,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(container.uptime_limit()).release(),
                    NULL);
    }
  }

  if (policy.has_start_up_flags()) {
    const em::StartUpFlagsProto& container(policy.start_up_flags());
    base::ListValue* flags = new base::ListValue();
    RepeatedPtrField<std::string>::const_iterator entry;
    for (entry = container.flags().begin();
         entry != container.flags().end();
         ++entry) {
      flags->Append(new base::StringValue(*entry));
    }
    policies->Set(key::kDeviceStartUpFlags,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD,
                  flags,
                  NULL);
  }

  if (policy.has_variations_parameter()) {
    if (policy.variations_parameter().has_parameter()) {
      policies->Set(key::kDeviceVariationsRestrictParameter,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::StringValue(
                        policy.variations_parameter().parameter()),
                    NULL);
    }
  }

  if (policy.has_attestation_settings()) {
    if (policy.attestation_settings().has_attestation_enabled()) {
      policies->Set(key::kAttestationEnabledForDevice,
                    POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    new base::FundamentalValue(
                        policy.attestation_settings().attestation_enabled()),
                    NULL);
    }
    if (policy.attestation_settings().has_content_protection_enabled()) {
      policies->Set(
          key::kAttestationForContentProtectionEnabled,
          POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          new base::FundamentalValue(
              policy.attestation_settings().content_protection_enabled()),
          NULL);
    }
  }

  if (policy.has_login_screen_power_management()) {
    const em::LoginScreenPowerManagementProto& container(
        policy.login_screen_power_management());
    if (container.has_login_screen_power_management()) {
      std::unique_ptr<base::Value> decoded_json;
      decoded_json = DecodeJsonStringAndDropUnknownBySchema(
          container.login_screen_power_management(),
          key::kDeviceLoginScreenPowerManagement);
      if (decoded_json) {
        policies->Set(key::kDeviceLoginScreenPowerManagement,
                      POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD,
                      decoded_json.release(),
                      NULL);
      }
    }
  }

  if (policy.has_system_settings()) {
    const em::SystemSettingsProto& container(policy.system_settings());
    if (container.has_block_devmode()) {
      policies->Set(
          key::kDeviceBlockDevmode,
          POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          new base::FundamentalValue(container.block_devmode()),
          NULL);
    }
  }

  if (policy.has_extension_cache_size()) {
    const em::ExtensionCacheSizeProto& container(policy.extension_cache_size());
    if (container.has_extension_cache_size()) {
      policies->Set(
          key::kExtensionCacheSize,
          POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          DecodeIntegerValue(container.extension_cache_size()).release(),
          nullptr);
    }
  }

  if (policy.has_login_screen_domain_auto_complete()) {
    const em::LoginScreenDomainAutoCompleteProto& container(
        policy.login_screen_domain_auto_complete());
    policies->Set(
        key::kDeviceLoginScreenDomainAutoComplete,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD,
        new base::StringValue(container.login_screen_domain_auto_complete()),
        nullptr);
  }

  if (policy.has_display_rotation_default()) {
    const em::DisplayRotationDefaultProto& container(
        policy.display_rotation_default());
    policies->Set(
        key::kDisplayRotationDefault,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD,
        DecodeIntegerValue(container.display_rotation_default()).release(),
        nullptr);
  }

  if (policy.has_usb_detachable_whitelist()) {
    const em::UsbDetachableWhitelistProto& container(
        policy.usb_detachable_whitelist());
    base::ListValue* whitelist = new base::ListValue();
    RepeatedPtrField<em::UsbDeviceIdProto>::const_iterator entry;
    for (entry = container.id().begin(); entry != container.id().end();
         ++entry) {
      base::DictionaryValue* ids = new base::DictionaryValue();
      if (entry->has_vendor_id()) {
        ids->SetString("vid", base::StringPrintf("%04X", entry->vendor_id()));
      }
      if (entry->has_product_id()) {
        ids->SetString("pid", base::StringPrintf("%04X", entry->product_id()));
      }
      whitelist->Append(ids);
    }
    policies->Set(key::kUsbDetachableWhitelist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, whitelist,
                  nullptr);
  }

  if (policy.has_quirks_download_enabled()) {
    const em::DeviceQuirksDownloadEnabledProto& container(
        policy.quirks_download_enabled());
    if (container.has_quirks_download_enabled()) {
      policies->Set(
          key::kDeviceQuirksDownloadEnabled,
          POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          new base::FundamentalValue(container.quirks_download_enabled()),
          nullptr);
    }
  }
}

}  // namespace

void DecodeDevicePolicy(const em::ChromeDeviceSettingsProto& policy,
                        PolicyMap* policies) {
  // Decode the various groups of policies.
  DecodeLoginPolicies(policy, policies);
  DecodeNetworkPolicies(policy, policies);
  DecodeReportingPolicies(policy, policies);
  DecodeAutoUpdatePolicies(policy, policies);
  DecodeAccessibilityPolicies(policy, policies);
  DecodeGenericPolicies(policy, policies);
}

}  // namespace policy
