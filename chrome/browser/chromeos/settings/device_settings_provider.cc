// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
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
#include "policy/proto/device_management_backend.pb.h"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

namespace em = enterprise_management;

namespace chromeos {

namespace {

// List of settings handled by the DeviceSettingsProvider.
const char* kKnownSettings[] = {
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
  kAllowRedeemChromeOsRegistrationOffers,
  kAllowedConnectionTypesForUpdate,
  kAppPack,
  kAttestationForContentProtectionEnabled,
  kDeviceAttestationEnabled,
  kDeviceOwner,
  kIdleLogoutTimeout,
  kIdleLogoutWarningDuration,
  kPolicyMissingMitigationMode,
  kReleaseChannel,
  kReleaseChannelDelegated,
  kReportDeviceActivityTimes,
  kReportDeviceBootMode,
  kReportDeviceLocation,
  kReportDeviceNetworkInterfaces,
  kReportDeviceUsers,
  kReportDeviceVersionInfo,
  kScreenSaverExtensionId,
  kScreenSaverTimeout,
  kServiceAccountIdentity,
  kSignedDataRoamingEnabled,
  kStartUpFlags,
  kStartUpUrls,
  kStatsReportingPref,
  kSystemTimezonePolicy,
  kSystemUse24HourClock,
  kUpdateDisabled,
  kVariationsRestrictParameter,
};

bool HasOldMetricsFile() {
  // TODO(pastarmovj): Remove this once migration is not needed anymore.
  // If the value is not set we should try to migrate legacy consent file.
  // Loading consent file state causes us to do blocking IO on UI thread.
  // Temporarily allow it until we fix http://crbug.com/62626
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  return GoogleUpdateSettings::GetCollectStatsConsent();
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
  device_settings_service_->RemoveObserver(this);
}

// static
bool DeviceSettingsProvider::IsDeviceSetting(const std::string& name) {
  const char** end = kKnownSettings + arraysize(kKnownSettings);
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

  if (IsDeviceSetting(path)) {
    pending_changes_.push_back(PendingQueueElement(path, in_value.DeepCopy()));
    if (!store_callback_factory_.HasWeakPtrs())
      SetInPolicy();
  } else {
    NOTREACHED() << "Try to set unhandled cros setting " << path;
  }
}

void DeviceSettingsProvider::OwnershipStatusChanged() {
  DeviceSettingsService::OwnershipStatus new_ownership_status =
      device_settings_service_->GetOwnershipStatus();

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
    StoreDeviceSettings();
  }

  // The owner key might have become available, allowing migration to happen.
  AttemptMigration();

  ownership_status_ = new_ownership_status;
}

void DeviceSettingsProvider::DeviceSettingsUpdated() {
  if (!store_callback_factory_.HasWeakPtrs())
    UpdateAndProceedStoring();
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

void DeviceSettingsProvider::SetInPolicy() {
  if (pending_changes_.empty()) {
    NOTREACHED();
    return;
  }

  if (RequestTrustedEntity() != TRUSTED) {
    // Re-sync device settings before proceeding.
    device_settings_service_->Load();
    return;
  }

  std::string prop(pending_changes_.front().first);
  scoped_ptr<base::Value> value(pending_changes_.front().second);
  pending_changes_.pop_front();

  trusted_status_ = TEMPORARILY_UNTRUSTED;
  if (prop == kAccountsPrefAllowNewUser) {
    em::AllowNewUsersProto* allow =
        device_settings_.mutable_allow_new_users();
    bool allow_value;
    if (value->GetAsBoolean(&allow_value))
      allow->set_allow_new_users(allow_value);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefAllowGuest) {
    em::GuestModeEnabledProto* guest =
        device_settings_.mutable_guest_mode_enabled();
    bool guest_value;
    if (value->GetAsBoolean(&guest_value))
      guest->set_guest_mode_enabled(guest_value);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefSupervisedUsersEnabled) {
    em::SupervisedUsersSettingsProto* supervised =
        device_settings_.mutable_supervised_users_settings();
    bool supervised_value;
    if (value->GetAsBoolean(&supervised_value))
      supervised->set_supervised_users_enabled(supervised_value);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefShowUserNamesOnSignIn) {
    em::ShowUserNamesOnSigninProto* show =
        device_settings_.mutable_show_user_names();
    bool show_value;
    if (value->GetAsBoolean(&show_value))
      show->set_show_user_names(show_value);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefDeviceLocalAccounts) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        device_settings_.mutable_device_local_accounts();
    device_local_accounts->clear_account();
    const base::ListValue* accounts_list = NULL;
    if (value->GetAsList(&accounts_list)) {
      for (base::ListValue::const_iterator entry(accounts_list->begin());
           entry != accounts_list->end(); ++entry) {
        const base::DictionaryValue* entry_dict = NULL;
        if ((*entry)->GetAsDictionary(&entry_dict)) {
          em::DeviceLocalAccountInfoProto* account =
              device_local_accounts->add_account();
          std::string account_id;
          if (entry_dict->GetStringWithoutPathExpansion(
                  kAccountsPrefDeviceLocalAccountsKeyId, &account_id)) {
            account->set_account_id(account_id);
          }
          int type;
          if (entry_dict->GetIntegerWithoutPathExpansion(
                  kAccountsPrefDeviceLocalAccountsKeyType, &type)) {
            account->set_type(
                static_cast<em::DeviceLocalAccountInfoProto::AccountType>(
                    type));
          }
          std::string kiosk_app_id;
          if (entry_dict->GetStringWithoutPathExpansion(
                  kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                  &kiosk_app_id)) {
            account->mutable_kiosk_app()->set_app_id(kiosk_app_id);
          }
        } else {
          NOTREACHED();
        }
      }
    } else {
      NOTREACHED();
    }
  } else if (prop == kAccountsPrefDeviceLocalAccountAutoLoginId) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        device_settings_.mutable_device_local_accounts();
    std::string id;
    if (value->GetAsString(&id))
      device_local_accounts->set_auto_login_id(id);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefDeviceLocalAccountAutoLoginDelay) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        device_settings_.mutable_device_local_accounts();
    int delay;
    if (value->GetAsInteger(&delay))
      device_local_accounts->set_auto_login_delay(delay);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        device_settings_.mutable_device_local_accounts();
    bool enabled;
    if (value->GetAsBoolean(&enabled))
      device_local_accounts->set_enable_auto_login_bailout(enabled);
    else
      NOTREACHED();
  } else if (prop ==
             kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        device_settings_.mutable_device_local_accounts();
    bool should_prompt;
    if (value->GetAsBoolean(&should_prompt))
      device_local_accounts->set_prompt_for_network_when_offline(should_prompt);
    else
      NOTREACHED();
  } else if (prop == kSignedDataRoamingEnabled) {
    em::DataRoamingEnabledProto* roam =
        device_settings_.mutable_data_roaming_enabled();
    bool roaming_value = false;
    if (value->GetAsBoolean(&roaming_value))
      roam->set_data_roaming_enabled(roaming_value);
    else
      NOTREACHED();
  } else if (prop == kReleaseChannel) {
    em::ReleaseChannelProto* release_channel =
        device_settings_.mutable_release_channel();
    std::string channel_value;
    if (value->GetAsString(&channel_value))
      release_channel->set_release_channel(channel_value);
    else
      NOTREACHED();
  } else if (prop == kStatsReportingPref) {
    em::MetricsEnabledProto* metrics =
        device_settings_.mutable_metrics_enabled();
    bool metrics_value = false;
    if (value->GetAsBoolean(&metrics_value))
      metrics->set_metrics_enabled(metrics_value);
    else
      NOTREACHED();
    ApplyMetricsSetting(false, metrics_value);
  } else if (prop == kAccountsPrefUsers) {
    em::UserWhitelistProto* whitelist_proto =
        device_settings_.mutable_user_whitelist();
    whitelist_proto->clear_user_whitelist();
    const base::ListValue* users;
    if (value->GetAsList(&users)) {
      for (base::ListValue::const_iterator i = users->begin();
           i != users->end(); ++i) {
        std::string email;
        if ((*i)->GetAsString(&email))
          whitelist_proto->add_user_whitelist(email);
      }
    }
  } else if (prop == kAccountsPrefEphemeralUsersEnabled) {
    em::EphemeralUsersEnabledProto* ephemeral_users_enabled =
        device_settings_.mutable_ephemeral_users_enabled();
    bool ephemeral_users_enabled_value = false;
    if (value->GetAsBoolean(&ephemeral_users_enabled_value)) {
      ephemeral_users_enabled->set_ephemeral_users_enabled(
          ephemeral_users_enabled_value);
    } else {
      NOTREACHED();
    }
  } else if (prop == kAllowRedeemChromeOsRegistrationOffers) {
    em::AllowRedeemChromeOsRegistrationOffersProto* allow_redeem_offers =
        device_settings_.mutable_allow_redeem_offers();
    bool allow_redeem_offers_value;
    if (value->GetAsBoolean(&allow_redeem_offers_value)) {
      allow_redeem_offers->set_allow_redeem_offers(
          allow_redeem_offers_value);
    } else {
      NOTREACHED();
    }
  } else if (prop == kStartUpFlags) {
    em::StartUpFlagsProto* flags_proto =
        device_settings_.mutable_start_up_flags();
    flags_proto->Clear();
    const base::ListValue* flags;
    if (value->GetAsList(&flags)) {
      for (base::ListValue::const_iterator i = flags->begin();
           i != flags->end(); ++i) {
        std::string flag;
        if ((*i)->GetAsString(&flag))
          flags_proto->add_flags(flag);
      }
    }
  } else if (prop == kSystemUse24HourClock) {
    em::SystemUse24HourClockProto* use_24hour_clock_proto =
        device_settings_.mutable_use_24hour_clock();
    use_24hour_clock_proto->Clear();
    bool use_24hour_clock_value;
    if (value->GetAsBoolean(&use_24hour_clock_value)) {
      use_24hour_clock_proto->set_use_24hour_clock(use_24hour_clock_value);
    } else {
      NOTREACHED();
    }
  } else if (prop == kAttestationForContentProtectionEnabled) {
    em::AttestationSettingsProto* attestation_settings =
        device_settings_.mutable_attestation_settings();
    bool setting_enabled;
    if (value->GetAsBoolean(&setting_enabled)) {
      attestation_settings->set_content_protection_enabled(setting_enabled);
    } else {
      NOTREACHED();
    }
  } else {
    // The remaining settings don't support Set(), since they are not
    // intended to be customizable by the user:
    //   kAccountsPrefTransferSAMLCookies
    //   kAppPack
    //   kDeviceAttestationEnabled
    //   kDeviceOwner
    //   kIdleLogoutTimeout
    //   kIdleLogoutWarningDuration
    //   kReleaseChannelDelegated
    //   kReportDeviceActivityTimes
    //   kReportDeviceBootMode
    //   kReportDeviceLocation
    //   kReportDeviceVersionInfo
    //   kReportDeviceNetworkInterfaces
    //   kReportDeviceUsers
    //   kScreenSaverExtensionId
    //   kScreenSaverTimeout
    //   kServiceAccountIdentity
    //   kStartUpUrls
    //   kSystemTimezonePolicy
    //   kVariationsRestrictParameter

    LOG(FATAL) << "Device setting " << prop << " is read-only.";
  }

  em::PolicyData data;
  data.set_username(device_settings_service_->GetUsername());
  CHECK(device_settings_.SerializeToString(data.mutable_policy_value()));

  // Set the cache to the updated value.
  UpdateValuesCache(data, device_settings_, trusted_status_);

  if (ownership_status_ == DeviceSettingsService::OWNERSHIP_TAKEN) {
    StoreDeviceSettings();
  } else {
    if (!device_settings_cache::Store(data, g_browser_process->local_state()))
      LOG(ERROR) << "Couldn't store to the temp storage.";

    // OnStorePolicyCompleted won't get called in this case so proceed with any
    // pending operations immediately.
    if (!pending_changes_.empty())
      SetInPolicy();
  }
}

void DeviceSettingsProvider::DecodeLoginPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) const {
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

  base::ListValue* list = new base::ListValue();
  const em::UserWhitelistProto& whitelist_proto = policy.user_whitelist();
  const RepeatedPtrField<std::string>& whitelist =
      whitelist_proto.user_whitelist();
  for (RepeatedPtrField<std::string>::const_iterator it = whitelist.begin();
       it != whitelist.end(); ++it) {
    list->Append(new base::StringValue(*it));
  }
  new_values_cache->SetValue(kAccountsPrefUsers, list);

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
    } else if (entry->has_deprecated_public_session_id()) {
      // Deprecated public session specification.
      entry_dict->SetStringWithoutPathExpansion(
          kAccountsPrefDeviceLocalAccountsKeyId,
          entry->deprecated_public_session_id());
      entry_dict->SetIntegerWithoutPathExpansion(
          kAccountsPrefDeviceLocalAccountsKeyType,
          policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION);
    }
    account_list->Append(entry_dict.release());
  }
  new_values_cache->SetValue(kAccountsPrefDeviceLocalAccounts,
                             account_list.release());

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
    base::ListValue* list = new base::ListValue();
    const em::StartUpFlagsProto& flags_proto = policy.start_up_flags();
    const RepeatedPtrField<std::string>& flags = flags_proto.flags();
    for (RepeatedPtrField<std::string>::const_iterator it = flags.begin();
         it != flags.end(); ++it) {
      list->Append(new base::StringValue(*it));
    }
    new_values_cache->SetValue(kStartUpFlags, list);
  }

  if (policy.has_saml_settings()) {
    new_values_cache->SetBoolean(
        kAccountsPrefTransferSAMLCookies,
        policy.saml_settings().transfer_saml_cookies());
  }
}

void DeviceSettingsProvider::DecodeKioskPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) const {
  if (policy.has_forced_logout_timeouts()) {
    if (policy.forced_logout_timeouts().has_idle_logout_timeout()) {
      new_values_cache->SetInteger(
          kIdleLogoutTimeout,
          policy.forced_logout_timeouts().idle_logout_timeout());
    }

    if (policy.forced_logout_timeouts().has_idle_logout_warning_duration()) {
      new_values_cache->SetInteger(
          kIdleLogoutWarningDuration,
          policy.forced_logout_timeouts().idle_logout_warning_duration());
    }
  }

  if (policy.has_login_screen_saver()) {
    if (policy.login_screen_saver().has_screen_saver_timeout()) {
      new_values_cache->SetInteger(
          kScreenSaverTimeout,
          policy.login_screen_saver().screen_saver_timeout());
    }

    if (policy.login_screen_saver().has_screen_saver_extension_id()) {
      new_values_cache->SetString(
          kScreenSaverExtensionId,
          policy.login_screen_saver().screen_saver_extension_id());
    }
  }

  if (policy.has_app_pack()) {
    typedef RepeatedPtrField<em::AppPackEntryProto> proto_type;
    base::ListValue* list = new base::ListValue;
    const proto_type& app_pack = policy.app_pack().app_pack();
    for (proto_type::const_iterator it = app_pack.begin();
         it != app_pack.end(); ++it) {
      base::DictionaryValue* entry = new base::DictionaryValue;
      if (it->has_extension_id()) {
        entry->SetStringWithoutPathExpansion(kAppPackKeyExtensionId,
                                             it->extension_id());
      }
      if (it->has_update_url()) {
        entry->SetStringWithoutPathExpansion(kAppPackKeyUpdateUrl,
                                             it->update_url());
      }
      list->Append(entry);
    }
    new_values_cache->SetValue(kAppPack, list);
  }

  if (policy.has_start_up_urls()) {
    base::ListValue* list = new base::ListValue();
    const em::StartUpUrlsProto& urls_proto = policy.start_up_urls();
    const RepeatedPtrField<std::string>& urls = urls_proto.start_up_urls();
    for (RepeatedPtrField<std::string>::const_iterator it = urls.begin();
         it != urls.end(); ++it) {
      list->Append(new base::StringValue(*it));
    }
    new_values_cache->SetValue(kStartUpUrls, list);
  }
}

void DeviceSettingsProvider::DecodeNetworkPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) const {
  // kSignedDataRoamingEnabled has a default value of false.
  new_values_cache->SetBoolean(
      kSignedDataRoamingEnabled,
      policy.has_data_roaming_enabled() &&
      policy.data_roaming_enabled().has_data_roaming_enabled() &&
      policy.data_roaming_enabled().data_roaming_enabled());
}

void DeviceSettingsProvider::DecodeAutoUpdatePolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) const {
  if (policy.has_auto_update_settings()) {
    const em::AutoUpdateSettingsProto& au_settings_proto =
        policy.auto_update_settings();
    if (au_settings_proto.has_update_disabled()) {
      new_values_cache->SetBoolean(kUpdateDisabled,
                                   au_settings_proto.update_disabled());
    }
    const RepeatedField<int>& allowed_connection_types =
        au_settings_proto.allowed_connection_types();
    base::ListValue* list = new base::ListValue();
    for (RepeatedField<int>::const_iterator i(allowed_connection_types.begin());
         i != allowed_connection_types.end(); ++i) {
      list->Append(new base::FundamentalValue(*i));
    }
    new_values_cache->SetValue(kAllowedConnectionTypesForUpdate, list);
  }
}

void DeviceSettingsProvider::DecodeReportingPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) const {
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
  }
}

void DeviceSettingsProvider::DecodeGenericPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) const {
  if (policy.has_metrics_enabled()) {
    new_values_cache->SetBoolean(kStatsReportingPref,
                                 policy.metrics_enabled().metrics_enabled());
  } else {
    new_values_cache->SetBoolean(kStatsReportingPref, HasOldMetricsFile());
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
}

void DeviceSettingsProvider::UpdateValuesCache(
    const em::PolicyData& policy_data,
    const em::ChromeDeviceSettingsProto& settings,
    TrustedStatus trusted_status) {
  PrefValueMap new_values_cache;

  if (policy_data.has_username() && !policy_data.has_request_token())
    new_values_cache.SetString(kDeviceOwner, policy_data.username());

  if (policy_data.has_service_account_identity()) {
    new_values_cache.SetString(kServiceAccountIdentity,
                               policy_data.service_account_identity());
  }

  DecodeLoginPolicies(settings, &new_values_cache);
  DecodeKioskPolicies(settings, &new_values_cache);
  DecodeNetworkPolicies(settings, &new_values_cache);
  DecodeAutoUpdatePolicies(settings, &new_values_cache);
  DecodeReportingPolicies(settings, &new_values_cache);
  DecodeGenericPolicies(settings, &new_values_cache);

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

void DeviceSettingsProvider::ApplyMetricsSetting(bool use_file,
                                                 bool new_value) {
  // TODO(pastarmovj): Remove this once migration is not needed anymore.
  // If the value is not set we should try to migrate legacy consent file.
  if (use_file) {
    new_value = HasOldMetricsFile();
    // Make sure the values will get eventually written to the policy file.
    migration_values_.SetBoolean(kStatsReportingPref, new_value);
    AttemptMigration();
    VLOG(1) << "No metrics policy set will revert to checking "
            << "consent file which is "
            << (new_value ? "on." : "off.");
    UMA_HISTOGRAM_COUNTS("DeviceSettings.MetricsMigrated", 1);
  }
  VLOG(1) << "Metrics policy is being set to : " << new_value
          << "(use file : " << use_file << ")";
  // TODO(pastarmovj): Remove this once we don't need to regenerate the
  // consent file for the GUID anymore.
  ResolveMetricsReportingEnabled(new_value);
}

void DeviceSettingsProvider::ApplySideEffects(
    const em::ChromeDeviceSettingsProto& settings) {
  // First migrate metrics settings as needed.
  if (settings.has_metrics_enabled())
    ApplyMetricsSetting(false, settings.metrics_enabled().metrics_enabled());
  else
    ApplyMetricsSetting(true, false);
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

  // Trigger the next change if necessary.
  if (trusted_status_ == TRUSTED && !pending_changes_.empty())
    SetInPolicy();
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

        // TODO(pastarmovj): Make those side effects responsibility of the
        // respective subsystems.
        ApplySideEffects(*device_settings);

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

void DeviceSettingsProvider::StoreDeviceSettings() {
  // Mute all previous callbacks to guarantee the |pending_changes_| queue is
  // processed serially.
  store_callback_factory_.InvalidateWeakPtrs();

  device_settings_service_->SignAndStore(
      scoped_ptr<em::ChromeDeviceSettingsProto>(
          new em::ChromeDeviceSettingsProto(device_settings_)),
      base::Bind(&DeviceSettingsProvider::UpdateAndProceedStoring,
                 store_callback_factory_.GetWeakPtr()));
}

void DeviceSettingsProvider::AttemptMigration() {
  if (device_settings_service_->HasPrivateOwnerKey()) {
    PrefValueMap::const_iterator i;
    for (i = migration_values_.begin(); i != migration_values_.end(); ++i)
      DoSet(i->first, *i->second);
    migration_values_.Clear();
  }
}

}  // namespace chromeos
