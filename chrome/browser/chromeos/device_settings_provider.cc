// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_settings_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/signed_settings_cache.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/policy/app_pack_updater.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/public/browser/notification_service.h"

using google::protobuf::RepeatedPtrField;

namespace em = enterprise_management;

namespace chromeos {

namespace {

// List of settings handled by the DeviceSettingsProvider.
const char* kKnownSettings[] = {
  kAccountsPrefAllowGuest,
  kAccountsPrefAllowNewUser,
  kAccountsPrefEphemeralUsersEnabled,
  kAccountsPrefShowUserNamesOnSignIn,
  kAccountsPrefUsers,
  kAppPack,
  kDeviceOwner,
  kIdleLogoutTimeout,
  kIdleLogoutWarningDuration,
  kPolicyMissingMitigationMode,
  kReleaseChannel,
  kReleaseChannelDelegated,
  kReportDeviceActivityTimes,
  kReportDeviceBootMode,
  kReportDeviceLocation,
  kReportDeviceVersionInfo,
  kScreenSaverExtensionId,
  kScreenSaverTimeout,
  kSettingProxyEverywhere,
  kSignedDataRoamingEnabled,
  kStartUpUrls,
  kStatsReportingPref,
};

// Upper bound for number of retries to fetch a signed setting.
static const int kNumRetriesLimit = 9;

// Legacy policy file location. Used to detect migration from pre v12 ChromeOS.
const char kLegacyPolicyFile[] = "/var/lib/whitelist/preferences";

bool IsControlledSetting(const std::string& pref_path) {
  const char** end = kKnownSettings + arraysize(kKnownSettings);
  return std::find(kKnownSettings, end, pref_path) != end;
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

DeviceSettingsProvider::DeviceSettingsProvider(
    const NotifyObserversCallback& notify_cb,
    SignedSettingsHelper* signed_settings_helper)
    : CrosSettingsProvider(notify_cb),
      signed_settings_helper_(signed_settings_helper),
      ownership_status_(OwnershipService::GetSharedInstance()->GetStatus(true)),
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
    signed_settings_helper_->StartRetrievePolicyOp(
        base::Bind(&DeviceSettingsProvider::OnRetrievePolicyCompleted,
                   base::Unretained(this)));
  }
}

void DeviceSettingsProvider::DoSet(const std::string& path,
                                   const base::Value& in_value) {
  if (!UserManager::Get()->IsCurrentUserOwner() &&
      ownership_status_ != OwnershipService::OWNERSHIP_NONE) {
    LOG(WARNING) << "Changing settings from non-owner, setting=" << path;

    // Revert UI change.
    NotifyObservers(path);
    return;
  }

  if (IsControlledSetting(path)) {
    pending_changes_.push_back(PendingQueueElement(path, in_value.DeepCopy()));
    if (pending_changes_.size() == 1)
      SetInPolicy();
  } else {
    NOTREACHED() << "Try to set unhandled cros setting " << path;
  }
}

void DeviceSettingsProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED) {
    // Reload the policy blob once the owner key has been loaded or updated.
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

void DeviceSettingsProvider::SetInPolicy() {
  if (pending_changes_.empty()) {
    NOTREACHED();
    return;
  }

  const std::string& prop = pending_changes_[0].first;
  base::Value* value = pending_changes_[0].second;
  if (prop == kDeviceOwner) {
    // Just store it in the memory cache without trusted checks or persisting.
    std::string owner;
    if (value->GetAsString(&owner)) {
      policy_.set_username(owner);
      // In this case the |value_cache_| takes the ownership of |value|.
      values_cache_.SetValue(prop, value);
      NotifyObservers(prop);
      // We can't trust this value anymore until we reload the real username.
      trusted_ = false;
      pending_changes_.erase(pending_changes_.begin());
      if (!pending_changes_.empty())
        SetInPolicy();
    } else {
      NOTREACHED();
    }
    return;
  }

  if (!RequestTrustedEntity()) {
    // Otherwise we should first reload and apply on top of that.
    signed_settings_helper_->StartRetrievePolicyOp(
        base::Bind(&DeviceSettingsProvider::FinishSetInPolicy,
                   base::Unretained(this)));
    return;
  }

  trusted_ = false;
  em::PolicyData data = policy();
  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(data.policy_value());
  if (prop == kAccountsPrefAllowNewUser) {
    em::AllowNewUsersProto* allow = pol.mutable_allow_new_users();
    bool allow_value;
    if (value->GetAsBoolean(&allow_value))
      allow->set_allow_new_users(allow_value);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefAllowGuest) {
    em::GuestModeEnabledProto* guest = pol.mutable_guest_mode_enabled();
    bool guest_value;
    if (value->GetAsBoolean(&guest_value))
      guest->set_guest_mode_enabled(guest_value);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefShowUserNamesOnSignIn) {
    em::ShowUserNamesOnSigninProto* show = pol.mutable_show_user_names();
    bool show_value;
    if (value->GetAsBoolean(&show_value))
      show->set_show_user_names(show_value);
    else
      NOTREACHED();
  } else if (prop == kSignedDataRoamingEnabled) {
    em::DataRoamingEnabledProto* roam = pol.mutable_data_roaming_enabled();
    bool roaming_value = false;
    if (value->GetAsBoolean(&roaming_value))
      roam->set_data_roaming_enabled(roaming_value);
    else
      NOTREACHED();
    ApplyRoamingSetting(roaming_value);
  } else if (prop == kSettingProxyEverywhere) {
    // TODO(cmasone): NOTIMPLEMENTED() once http://crosbug.com/13052 is fixed.
    std::string proxy_value;
    if (value->GetAsString(&proxy_value)) {
      bool success =
          pol.mutable_device_proxy_settings()->ParseFromString(proxy_value);
      DCHECK(success);
    } else {
      NOTREACHED();
    }
  } else if (prop == kReleaseChannel) {
    em::ReleaseChannelProto* release_channel = pol.mutable_release_channel();
    std::string channel_value;
    if (value->GetAsString(&channel_value))
      release_channel->set_release_channel(channel_value);
    else
      NOTREACHED();
  } else if (prop == kStatsReportingPref) {
    em::MetricsEnabledProto* metrics = pol.mutable_metrics_enabled();
    bool metrics_value = false;
    if (value->GetAsBoolean(&metrics_value))
      metrics->set_metrics_enabled(metrics_value);
    else
      NOTREACHED();
    ApplyMetricsSetting(false, metrics_value);
  } else if (prop == kAccountsPrefUsers) {
    em::UserWhitelistProto* whitelist_proto = pol.mutable_user_whitelist();
    whitelist_proto->clear_user_whitelist();
    base::ListValue& users = static_cast<base::ListValue&>(*value);
    for (base::ListValue::const_iterator i = users.begin();
         i != users.end(); ++i) {
      std::string email;
      if ((*i)->GetAsString(&email))
        whitelist_proto->add_user_whitelist(email.c_str());
    }
  } else if (prop == kAccountsPrefEphemeralUsersEnabled) {
    em::EphemeralUsersEnabledProto* ephemeral_users_enabled =
        pol.mutable_ephemeral_users_enabled();
    bool ephemeral_users_enabled_value = false;
    if (value->GetAsBoolean(&ephemeral_users_enabled_value))
      ephemeral_users_enabled->set_ephemeral_users_enabled(
          ephemeral_users_enabled_value);
    else
      NOTREACHED();
  } else {
    // The remaining settings don't support Set(), since they are not
    // intended to be customizable by the user:
    //   kAppPack
    //   kIdleLogoutTimeout
    //   kIdleLogoutWarningDuration
    //   kReleaseChannelDelegated
    //   kReportDeviceVersionInfo
    //   kReportDeviceActivityTimes
    //   kReportDeviceBootMode
    //   kReportDeviceLocation
    //   kScreenSaverExtensionId
    //   kScreenSaverTimeout
    //   kStartUpUrls

    NOTREACHED();
  }
  data.set_policy_value(pol.SerializeAsString());
  // Set the cache to the updated value.
  policy_ = data;
  UpdateValuesCache();

  if (!signed_settings_cache::Store(data, g_browser_process->local_state()))
    LOG(ERROR) << "Couldn't store to the temp storage.";

  if (ownership_status_ == OwnershipService::OWNERSHIP_TAKEN) {
    em::PolicyFetchResponse policy_envelope;
    policy_envelope.set_policy_data(policy_.SerializeAsString());
    signed_settings_helper_->StartStorePolicyOp(
        policy_envelope,
        base::Bind(&DeviceSettingsProvider::OnStorePolicyCompleted,
                   base::Unretained(this)));
  } else {
    // OnStorePolicyCompleted won't get called in this case so proceed with any
    // pending operations immediately.
    delete pending_changes_[0].second;
    pending_changes_.erase(pending_changes_.begin());
    if (!pending_changes_.empty())
      SetInPolicy();
  }
}

void DeviceSettingsProvider::FinishSetInPolicy(
    SignedSettings::ReturnCode code,
    const em::PolicyFetchResponse& policy) {
  if (code != SignedSettings::SUCCESS) {
    LOG(ERROR) << "Can't serialize to policy error code: " << code;
    Reload();
    return;
  }
  // Update the internal caches and set the trusted flag to true so that we
  // can pass the trustedness check in the second call to SetInPolicy.
  OnRetrievePolicyCompleted(code, policy);

  SetInPolicy();
}

void DeviceSettingsProvider::DecodeLoginPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) const {
  // For all our boolean settings the following is applicable:
  // true is default permissive value and false is safe prohibitive value.
  // Exceptions:
  //   kSignedDataRoamingEnabled has a default value of false.
  //   kAccountsPrefEphemeralUsersEnabled has a default value of false.
  if (policy.has_allow_new_users() &&
      policy.allow_new_users().has_allow_new_users() &&
      policy.allow_new_users().allow_new_users()) {
    // New users allowed, user_whitelist() ignored.
    new_values_cache->SetBoolean(kAccountsPrefAllowNewUser, true);
  } else if (!policy.has_user_whitelist()) {
    // If we have the allow_new_users bool, and it is true, we honor that above.
    // In all other cases (don't have it, have it and it is set to false, etc),
    // We will honor the user_whitelist() if it is there and populated.
    // Otherwise we default to allowing new users.
    new_values_cache->SetBoolean(kAccountsPrefAllowNewUser, true);
  } else {
    new_values_cache->SetBoolean(
        kAccountsPrefAllowNewUser,
        policy.user_whitelist().user_whitelist_size() == 0);
  }

  new_values_cache->SetBoolean(
      kAccountsPrefAllowGuest,
      !policy.has_guest_mode_enabled() ||
      !policy.guest_mode_enabled().has_guest_mode_enabled() ||
      policy.guest_mode_enabled().guest_mode_enabled());

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
    list->Append(base::Value::CreateStringValue(*it));
  }
  new_values_cache->SetValue(kAccountsPrefUsers, list);
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
        entry->SetString(policy::AppPackUpdater::kExtensionId,
                         it->extension_id());
      }
      if (it->has_update_url())
        entry->SetString(policy::AppPackUpdater::kUpdateUrl, it->update_url());
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
      list->Append(base::Value::CreateStringValue(*it));
    }
    new_values_cache->SetValue(kStartUpUrls, list);
  }
}

void DeviceSettingsProvider::DecodeNetworkPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) const {
  new_values_cache->SetBoolean(
      kSignedDataRoamingEnabled,
      policy.has_data_roaming_enabled() &&
      policy.data_roaming_enabled().has_data_roaming_enabled() &&
      policy.data_roaming_enabled().data_roaming_enabled());

  // TODO(cmasone): NOTIMPLEMENTED() once http://crosbug.com/13052 is fixed.
  std::string serialized;
  if (policy.has_device_proxy_settings() &&
      policy.device_proxy_settings().SerializeToString(&serialized)) {
    new_values_cache->SetString(kSettingProxyEverywhere, serialized);
  }
}

void DeviceSettingsProvider::DecodeReportingPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) const {
  if (policy.has_device_reporting()) {
    if (policy.device_reporting().has_report_version_info()) {
      new_values_cache->SetBoolean(
          kReportDeviceVersionInfo,
          policy.device_reporting().report_version_info());
    }
    if (policy.device_reporting().has_report_activity_times()) {
      new_values_cache->SetBoolean(
          kReportDeviceActivityTimes,
          policy.device_reporting().report_activity_times());
    }
    if (policy.device_reporting().has_report_boot_mode()) {
      new_values_cache->SetBoolean(
          kReportDeviceBootMode,
          policy.device_reporting().report_boot_mode());
    }
    if (policy.device_reporting().has_report_location()) {
      new_values_cache->SetBoolean(
          kReportDeviceLocation,
          policy.device_reporting().report_location());
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
}

void DeviceSettingsProvider::UpdateValuesCache() {
  const em::PolicyData data = policy();
  PrefValueMap new_values_cache;

  if (data.has_username() && !data.has_request_token())
    new_values_cache.SetString(kDeviceOwner, data.username());

  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(data.policy_value());

  DecodeLoginPolicies(pol, &new_values_cache);
  DecodeKioskPolicies(pol, &new_values_cache);
  DecodeNetworkPolicies(pol, &new_values_cache);
  DecodeReportingPolicies(pol, &new_values_cache);
  DecodeGenericPolicies(pol, &new_values_cache);

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
  for (size_t i = 0; i < notifications.size(); ++i)
    NotifyObservers(notifications[i]);
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

bool DeviceSettingsProvider::MitigateMissingPolicy() {
  // First check if the device has been owned already and if not exit
  // immediately.
  if (g_browser_process->browser_policy_connector()->GetDeviceMode() !=
      policy::DEVICE_MODE_CONSUMER) {
    return false;
  }

  // If we are here the policy file were corrupted or missing. This can happen
  // because we are migrating Pre R11 device to the new secure policies or there
  // was an attempt to circumvent policy system. In this case we should populate
  // the policy cache with "safe-mode" defaults which should allow the owner to
  // log in but lock the device for anyone else until the policy blob has been
  // recreated by the session manager.
  LOG(ERROR) << "Corruption of the policy data has been detected."
             << "Switching to \"safe-mode\" policies until the owner logs in "
             << "to regenerate the policy data.";
  values_cache_.SetBoolean(kAccountsPrefAllowNewUser, true);
  values_cache_.SetBoolean(kAccountsPrefAllowGuest, true);
  values_cache_.SetBoolean(kPolicyMissingMitigationMode, true);
  trusted_ = true;
  // Make sure we will recreate the policy once the owner logs in.
  // Any value not in this list will be left to the default which is fine as
  // we repopulate the whitelist with the owner and all other existing users
  // every time the owner enables whitelist filtering on the UI.
  migration_helper_->AddMigrationValue(
      kAccountsPrefAllowNewUser, base::Value::CreateBooleanValue(true));
  migration_helper_->MigrateValues();
  // The last step is to pretend we loaded policy correctly and call everyone.
  for (size_t i = 0; i < callbacks_.size(); ++i)
    callbacks_[i].Run();
  callbacks_.clear();
  return true;
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

bool DeviceSettingsProvider::PrepareTrustedValues(const base::Closure& cb) {
  if (RequestTrustedEntity())
    return true;
  if (!cb.is_null())
    callbacks_.push_back(cb);
  return false;
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

  // Clear the finished task and proceed with any other stores that could be
  // pending by now.
  delete pending_changes_[0].second;
  pending_changes_.erase(pending_changes_.begin());
  if (!pending_changes_.empty())
    SetInPolicy();
}

void DeviceSettingsProvider::OnRetrievePolicyCompleted(
    SignedSettings::ReturnCode code,
    const em::PolicyFetchResponse& policy_data) {
  VLOG(1) << "OnRetrievePolicyCompleted. Error code: " << code
          << ", trusted : " << trusted_ << ", status : " << ownership_status_;
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
      if (MitigateMissingPolicy())
        break;
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
