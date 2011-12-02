// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_settings_provider.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/signed_settings_cache.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;
using google::protobuf::RepeatedPtrField;

namespace chromeos {

namespace {

const char* kBooleanSettings[] = {
  kAccountsPrefAllowNewUser,
  kAccountsPrefAllowGuest,
  kAccountsPrefShowUserNamesOnSignIn,
  kSignedDataRoamingEnabled,
  kStatsReportingPref
};

const char* kStringSettings[] = {
  kDeviceOwner,
  kReleaseChannel,
  kSettingProxyEverywhere
};

const char* kListSettings[] = {
  kAccountsPrefUsers
};

// Upper bound for number of retries to fetch a signed setting.
static const int kNumRetriesLimit = 9;

bool IsControlledBooleanSetting(const std::string& pref_path) {
  const char** end = kBooleanSettings + arraysize(kBooleanSettings);
  return std::find(kBooleanSettings, end, pref_path) != end;
}

bool IsControlledStringSetting(const std::string& pref_path) {
  const char** end = kStringSettings + arraysize(kStringSettings);
  return std::find(kStringSettings, end, pref_path) != end;
}

bool IsControlledListSetting(const std::string& pref_path) {
  const char** end = kListSettings + arraysize(kListSettings);
  return std::find(kListSettings, end, pref_path) != end;
}

bool IsControlledSetting(const std::string& pref_path) {
  return (IsControlledBooleanSetting(pref_path) ||
          IsControlledStringSetting(pref_path) ||
          IsControlledListSetting(pref_path));
}

bool HasOldMetricsFile() {
  // TODO(pastarmovj): Remove this once migration is not needed anymore.
  // If the value is not set we should try to migrate legacy consent file.
  // Loading consent file state causes us to do blocking IO on UI thread.
  // Temporarily allow it until we fix http://crbug.com/62626
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  return GoogleUpdateSettings::GetCollectStatsConsent();
}

}  // namespace

DeviceSettingsProvider::DeviceSettingsProvider()
    : ownership_status_(OwnershipService::GetSharedInstance()->GetStatus(true)),
      migration_helper_(new SignedSettingsMigrationHelper()),
      retries_left_(kNumRetriesLimit),
      trusted_(false) {
  // Register for notification when ownership is taken so that we can update
  // the |ownership_status_| and reload if needed.
  registrar_.Add(this, chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED,
                 content::NotificationService::AllSources());
  // Make sure we have at least the cache data immediately.
  RetrieveCachedData();
  // Start prefetching preferences.
  Reload();
}

DeviceSettingsProvider::~DeviceSettingsProvider() {
}

void DeviceSettingsProvider::Reload() {
  // While fetching we can't trust the cache anymore.
  trusted_ = false;
  if (ownership_status_ == OwnershipService::OWNERSHIP_NONE) {
    RetrieveCachedData();
  } else {
    // Retrieve the real data.
    SignedSettingsHelper::Get()->StartRetrievePolicyOp(
        base::Bind(&DeviceSettingsProvider::OnRetrievePolicyCompleted,
                   base::Unretained(this)));
  }
}

void DeviceSettingsProvider::DoSet(const std::string& path,
                                   const base::Value& in_value) {
  if (!UserManager::Get()->current_user_is_owner() &&
      ownership_status_ != OwnershipService::OWNERSHIP_NONE) {
    LOG(WARNING) << "Changing settings from non-owner, setting=" << path;

    // Revert UI change.
    CrosSettings::Get()->FireObservers(path.c_str());
    return;
  }

  if (IsControlledSetting(path))
    SetInPolicy(path, in_value);
  else
    NOTREACHED() << "Try to set unhandled cros setting " << path;
}

void DeviceSettingsProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED &&
      UserManager::Get()->current_user_is_owner()) {
    // Reload the initial policy blob, apply settings from temp storage,
    // and write back the blob.
    ownership_status_ = OwnershipService::OWNERSHIP_TAKEN;
    Reload();
  }
}

const em::PolicyData DeviceSettingsProvider::policy() const {
  return policy_;
}

void DeviceSettingsProvider::RetrieveCachedData() {
  // If there is no owner yet, this function will pull the policy cache from the
  // temp storage and use that instead.
  em::PolicyData policy;
  if (!signed_settings_cache::Retrieve(&policy,
                                       g_browser_process->local_state())) {
    VLOG(1) << "Can't retrieve temp store possibly not created yet.";
    // Prepare empty data for the case we don't have temp cache yet.
    policy.set_policy_type(kDevicePolicyType);
    em::ChromeDeviceSettingsProto pol;
    policy.set_policy_value(pol.SerializeAsString());
  }

  policy_ = policy;
  UpdateValuesCache();
}

void DeviceSettingsProvider::SetInPolicy(const std::string& prop,
                                         const base::Value& value) {
  if (prop == kDeviceOwner) {
    // Just store it in the memory cache without trusted checks or persisting.
    std::string owner;
    if (value.GetAsString(&owner)) {
      policy_.set_username(owner);
      values_cache_.SetValue(prop, value.DeepCopy());
      CrosSettings::Get()->FireObservers(prop.c_str());
      // We can't trust this value anymore until we reload the real username.
      trusted_ = false;
    } else {
      NOTREACHED();
    }
    return;
  }

  if (!RequestTrustedEntity()) {
    // Otherwise we should first reload and apply on top of that.
    SignedSettingsHelper::Get()->StartRetrievePolicyOp(
            base::Bind(&DeviceSettingsProvider::FinishSetInPolicy,
                       base::Unretained(this),
                       prop, base::Owned(value.DeepCopy())));
    return;
  }

  trusted_ = false;
  em::PolicyData data = policy();
  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(data.policy_value());
  if (prop == kAccountsPrefAllowNewUser) {
    em::AllowNewUsersProto* allow = pol.mutable_allow_new_users();
    bool allow_value;
    if (value.GetAsBoolean(&allow_value))
      allow->set_allow_new_users(allow_value);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefAllowGuest) {
    em::GuestModeEnabledProto* guest = pol.mutable_guest_mode_enabled();
    bool guest_value;
    if (value.GetAsBoolean(&guest_value))
      guest->set_guest_mode_enabled(guest_value);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefShowUserNamesOnSignIn) {
    em::ShowUserNamesOnSigninProto* show = pol.mutable_show_user_names();
    bool show_value;
    if (value.GetAsBoolean(&show_value))
      show->set_show_user_names(show_value);
    else
      NOTREACHED();
  } else if (prop == kSignedDataRoamingEnabled) {
    em::DataRoamingEnabledProto* roam = pol.mutable_data_roaming_enabled();
    bool roaming_value = false;
    if (value.GetAsBoolean(&roaming_value))
      roam->set_data_roaming_enabled(roaming_value);
    else
      NOTREACHED();
    ApplyRoamingSetting(roaming_value);
  } else if (prop == kSettingProxyEverywhere) {
    // TODO(cmasone): NOTIMPLEMENTED() once http://crosbug.com/13052 is fixed.
    std::string proxy_value;
    if (value.GetAsString(&proxy_value)) {
      bool success =
          pol.mutable_device_proxy_settings()->ParseFromString(proxy_value);
      DCHECK(success);
    } else {
      NOTREACHED();
    }
  } else if (prop == kReleaseChannel) {
    em::ReleaseChannelProto* release_channel = pol.mutable_release_channel();
    std::string channel_value;
    if (value.GetAsString(&channel_value))
      release_channel->set_release_channel(channel_value);
    else
      NOTREACHED();
  } else if (prop == kStatsReportingPref) {
    em::MetricsEnabledProto* metrics = pol.mutable_metrics_enabled();
    bool metrics_value = false;
    if (value.GetAsBoolean(&metrics_value))
      metrics->set_metrics_enabled(metrics_value);
    else
      NOTREACHED();
    ApplyMetricsSetting(false, metrics_value);
  } else if (prop == kAccountsPrefUsers) {
    em::UserWhitelistProto* whitelist_proto = pol.mutable_user_whitelist();
    whitelist_proto->clear_user_whitelist();
    const base::ListValue& users = static_cast<const base::ListValue&>(value);
    for (base::ListValue::const_iterator i = users.begin();
         i != users.end(); ++i) {
      std::string email;
      if ((*i)->GetAsString(&email))
        whitelist_proto->add_user_whitelist(email.c_str());
    }
  } else {
    NOTREACHED();
  }
  data.set_policy_value(pol.SerializeAsString());
  // Set the cache to the updated value.
  policy_ = data;
  UpdateValuesCache();
  CrosSettings::Get()->FireObservers(prop.c_str());

  if (!signed_settings_cache::Store(data, g_browser_process->local_state()))
    LOG(ERROR) << "Couldn't store to the temp storage.";

  if (ownership_status_ == OwnershipService::OWNERSHIP_TAKEN) {
    em::PolicyFetchResponse policy_envelope;
    policy_envelope.set_policy_data(policy_.SerializeAsString());
    SignedSettingsHelper::Get()->StartStorePolicyOp(
        policy_envelope,
        base::Bind(&DeviceSettingsProvider::OnStorePolicyCompleted,
                   base::Unretained(this)));
  }
}

void DeviceSettingsProvider::FinishSetInPolicy(
    const std::string& prop,
    const base::Value* value,
    SignedSettings::ReturnCode code,
    const em::PolicyFetchResponse& policy) {
  if (code != SignedSettings::SUCCESS) {
    LOG(ERROR) << "Can't serialize to policy error code: " << code;
    Reload();
    return;
  }
  SetInPolicy(prop, *value);
}

void DeviceSettingsProvider::UpdateValuesCache() {
  const em::PolicyData data = policy();
  values_cache_.Clear();

  if (data.has_username() && !data.has_request_token())
    values_cache_.SetString(kDeviceOwner, data.username());

  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(data.policy_value());

  // For all our boolean settings the following is applicable:
  // true is default permissive value and false is safe prohibitive value.
  // Exception: kSignedDataRoamingEnabled which has default value of false.
  if (pol.has_allow_new_users() &&
      pol.allow_new_users().has_allow_new_users() &&
      pol.allow_new_users().allow_new_users()) {
    // New users allowed, user_whitelist() ignored.
    values_cache_.SetBoolean(kAccountsPrefAllowNewUser, true);
  } else if (!pol.has_user_whitelist()) {
    // If we have the allow_new_users bool, and it is true, we honor that above.
    // In all other cases (don't have it, have it and it is set to false, etc),
    // We will honor the user_whitelist() if it is there and populated.
    // Otherwise we default to allowing new users.
    values_cache_.SetBoolean(kAccountsPrefAllowNewUser, true);
  } else {
    values_cache_.SetBoolean(kAccountsPrefAllowNewUser,
                             pol.user_whitelist().user_whitelist_size() == 0);
  }

  values_cache_.SetBoolean(
      kAccountsPrefAllowGuest,
      !pol.has_guest_mode_enabled() ||
      !pol.guest_mode_enabled().has_guest_mode_enabled() ||
      pol.guest_mode_enabled().guest_mode_enabled());

  values_cache_.SetBoolean(
      kAccountsPrefShowUserNamesOnSignIn,
      !pol.has_show_user_names() ||
      !pol.show_user_names().has_show_user_names() ||
      pol.show_user_names().show_user_names());

  values_cache_.SetBoolean(
      kSignedDataRoamingEnabled,
      pol.has_data_roaming_enabled() &&
      pol.data_roaming_enabled().has_data_roaming_enabled() &&
      pol.data_roaming_enabled().data_roaming_enabled());

  // TODO(cmasone): NOTIMPLEMENTED() once http://crosbug.com/13052 is fixed.
  std::string serialized;
  if (pol.has_device_proxy_settings() &&
      pol.device_proxy_settings().SerializeToString(&serialized)) {
    values_cache_.SetString(kSettingProxyEverywhere, serialized);
  }

  if (!pol.has_release_channel() ||
      !pol.release_channel().has_release_channel()) {
    // Default to an invalid channel (will be ignored).
    values_cache_.SetString(kReleaseChannel, "");
  } else {
    values_cache_.SetString(kReleaseChannel,
                            pol.release_channel().release_channel());
  }

  if (pol.has_metrics_enabled()) {
    values_cache_.SetBoolean(kStatsReportingPref,
                             pol.metrics_enabled().metrics_enabled());
  } else {
    values_cache_.SetBoolean(kStatsReportingPref, HasOldMetricsFile());
  }

  base::ListValue* list = new base::ListValue();
  const em::UserWhitelistProto& whitelist_proto = pol.user_whitelist();
  const RepeatedPtrField<std::string>& whitelist =
      whitelist_proto.user_whitelist();
  for (RepeatedPtrField<std::string>::const_iterator it = whitelist.begin();
       it != whitelist.end(); ++it) {
    list->Append(base::Value::CreateStringValue(*it));
  }
  values_cache_.SetValue(kAccountsPrefUsers, list);
}

void DeviceSettingsProvider::ApplyMetricsSetting(bool use_file,
                                                 bool new_value) const {
  // TODO(pastarmovj): Remove this once migration is not needed anymore.
  // If the value is not set we should try to migrate legacy consent file.
  if (use_file) {
    new_value = HasOldMetricsFile();
    // Make sure the values will get eventually written to the policy file.
    migration_helper_->AddMigrationValue(
        kStatsReportingPref, base::Value::CreateBooleanValue(new_value));
    migration_helper_->MigrateValues();
    LOG(INFO) << "No metrics policy set will revert to checking "
                 << "consent file which is "
                 << (new_value ? "on." : "off.");
  }
  VLOG(1) << "Metrics policy is being set to : " << new_value
          << "(use file : " << use_file << ")";
  // TODO(pastarmovj): Remove this once we don't need to regenerate the
  // consent file for the GUID anymore.
  OptionsUtil::ResolveMetricsReportingEnabled(new_value);
}

void DeviceSettingsProvider::ApplyRoamingSetting(bool new_value) const {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  const NetworkDevice* cellular = cros->FindCellularDevice();
  if (cellular) {
    bool device_value = cellular->data_roaming_allowed();
    if (!device_value && cros->IsCellularAlwaysInRoaming()) {
      // If operator requires roaming always enabled, ignore supplied value
      // and set data roaming allowed in true always.
      cros->SetCellularDataRoamingAllowed(true);
    } else if (device_value != new_value) {
      cros->SetCellularDataRoamingAllowed(new_value);
    }
  }
}

void DeviceSettingsProvider::ApplySideEffects() const {
  const em::PolicyData data = policy();
  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(data.policy_value());
  // First migrate metrics settings as needed.
  if (pol.has_metrics_enabled())
    ApplyMetricsSetting(false, pol.metrics_enabled().metrics_enabled());
  else
    ApplyMetricsSetting(true, false);
  // Next set the roaming setting as needed.
  ApplyRoamingSetting(pol.has_data_roaming_enabled() ?
      pol.data_roaming_enabled().data_roaming_enabled() : false);
}

const base::Value* DeviceSettingsProvider::Get(const std::string& path) const {
  if (IsControlledSetting(path)) {
    const base::Value* value;
    if (values_cache_.GetValue(path, &value))
      return value;
  } else {
    NOTREACHED() << "Trying to get non cros setting.";
  }

  return NULL;
}

bool DeviceSettingsProvider::GetTrusted(const std::string& path,
                                        const base::Closure& callback) {
  if (!IsControlledSetting(path)) {
    NOTREACHED();
    return true;
  }

  if (RequestTrustedEntity()) {
    return true;
  } else {
    if (!callback.is_null())
      callbacks_.push_back(callback);
    return false;
  }
}

bool DeviceSettingsProvider::HandlesSetting(const std::string& path) const {
  return IsControlledSetting(path);
}

bool DeviceSettingsProvider::RequestTrustedEntity() {
  if (ownership_status_ == OwnershipService::OWNERSHIP_NONE)
    return true;
  return trusted_;
}

void DeviceSettingsProvider::OnStorePolicyCompleted(
    SignedSettings::ReturnCode code) {
  // In any case reload the policy cache to now.
  if (code != SignedSettings::SUCCESS)
    Reload();
  else
    trusted_ = true;
}

void DeviceSettingsProvider::OnRetrievePolicyCompleted(
    SignedSettings::ReturnCode code,
    const em::PolicyFetchResponse& policy_data) {
  switch (code) {
    case SignedSettings::SUCCESS: {
      DCHECK(policy_data.has_policy_data());
      policy_.ParseFromString(policy_data.policy_data());
      signed_settings_cache::Store(policy(),
                                   g_browser_process->local_state());
      UpdateValuesCache();
      trusted_ = true;
      for (size_t i = 0; i < callbacks_.size(); ++i)
        callbacks_[i].Run();
      callbacks_.clear();
      // TODO(pastarmovj): Make those side effects responsibility of the
      // respective subsystems.
      ApplySideEffects();
      break;
    }
    case SignedSettings::NOT_FOUND:
    case SignedSettings::KEY_UNAVAILABLE: {
      if (ownership_status_ != OwnershipService::OWNERSHIP_TAKEN)
        NOTREACHED() << "No policies present yet, will use the temp storage.";
      break;
    }
    case SignedSettings::BAD_SIGNATURE:
    case SignedSettings::OPERATION_FAILED: {
      LOG(ERROR) << "Failed to retrieve cros policies. Reason:" << code;
      if (retries_left_ > 0) {
        retries_left_ -= 1;
        Reload();
        return;
      }
      LOG(ERROR) << "No retries left";
      break;
    }
  }
}

}  // namespace chromeos
