// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"

namespace syncer {

namespace {

// Obsolete prefs related to the removed ClearServerData flow.
const char kSyncPassphraseEncryptionTransitionInProgress[] =
    "sync.passphrase_encryption_transition_in_progress";
const char kSyncNigoriStateForPassphraseTransition[] =
    "sync.nigori_state_for_passphrase_transition";

// Obsolete pref that used to store a bool on whether Sync has an auth error.
const char kSyncHasAuthError[] = "sync.has_auth_error";

// Obsolete pref that used to store the timestamp of first sync.
const char kSyncFirstSyncTime[] = "sync.first_sync_time";

// Obsolete pref that used to store long poll intervals received by the server.
const char kSyncLongPollIntervalSeconds[] = "sync.long_poll_interval";

// Groups of prefs that always have the same value as a "master" pref.
// For example, the APPS group has {APP_LIST, APP_SETTINGS}
// (as well as APPS, but that is implied), so
//   pref_groups_[APPS] =       { APP_LIST, APP_SETTINGS }
//   pref_groups_[EXTENSIONS] = { EXTENSION_SETTINGS }
// etc.
using PrefGroupsMap = std::map<ModelType, ModelTypeSet>;
PrefGroupsMap ComputePrefGroups() {
  PrefGroupsMap pref_groups;
  pref_groups[APPS].Put(APP_SETTINGS);
  pref_groups[APPS].Put(APP_LIST);
  pref_groups[APPS].Put(ARC_PACKAGE);

  pref_groups[AUTOFILL].Put(AUTOFILL_PROFILE);
  pref_groups[AUTOFILL].Put(AUTOFILL_WALLET_DATA);
  pref_groups[AUTOFILL].Put(AUTOFILL_WALLET_METADATA);

  pref_groups[EXTENSIONS].Put(EXTENSION_SETTINGS);

  pref_groups[PREFERENCES].Put(DICTIONARY);
  pref_groups[PREFERENCES].Put(PRIORITY_PREFERENCES);
  pref_groups[PREFERENCES].Put(SEARCH_ENGINES);

  pref_groups[TYPED_URLS].Put(HISTORY_DELETE_DIRECTIVES);
  pref_groups[TYPED_URLS].Put(SESSIONS);
  pref_groups[TYPED_URLS].Put(FAVICON_IMAGES);
  pref_groups[TYPED_URLS].Put(FAVICON_TRACKING);
  pref_groups[TYPED_URLS].Put(USER_EVENTS);

  pref_groups[PROXY_TABS].Put(SESSIONS);
  pref_groups[PROXY_TABS].Put(FAVICON_IMAGES);
  pref_groups[PROXY_TABS].Put(FAVICON_TRACKING);

  return pref_groups;
}

std::vector<std::string> GetObsoleteUserTypePrefs() {
  return {prefs::kSyncAutofillProfile,
          prefs::kSyncAutofillWallet,
          prefs::kSyncAutofillWalletMetadata,
          prefs::kSyncSearchEngines,
          prefs::kSyncSessions,
          prefs::kSyncAppSettings,
          prefs::kSyncExtensionSettings,
          prefs::kSyncAppNotifications,
          prefs::kSyncHistoryDeleteDirectives,
          prefs::kSyncSyncedNotifications,
          prefs::kSyncSyncedNotificationAppInfo,
          prefs::kSyncDictionary,
          prefs::kSyncFaviconImages,
          prefs::kSyncFaviconTracking,
          prefs::kSyncDeviceInfo,
          prefs::kSyncPriorityPreferences,
          prefs::kSyncSupervisedUserSettings,
          prefs::kSyncSupervisedUsers,
          prefs::kSyncSupervisedUserSharedSettings,
          prefs::kSyncArticles,
          prefs::kSyncAppList,
          prefs::kSyncWifiCredentials,
          prefs::kSyncSupervisedUserWhitelists,
          prefs::kSyncArcPackage,
          prefs::kSyncUserEvents,
          prefs::kSyncMountainShares,
          prefs::kSyncUserConsents,
          prefs::kSyncSendTabToSelf};
}

void RegisterObsoleteUserTypePrefs(user_prefs::PrefRegistrySyncable* registry) {
  for (const std::string& obsolete_pref : GetObsoleteUserTypePrefs()) {
    registry->RegisterBooleanPref(obsolete_pref, false);
  }
}

}  // namespace

CryptoSyncPrefs::~CryptoSyncPrefs() {}

SyncPrefObserver::~SyncPrefObserver() {}

SyncPrefs::SyncPrefs(PrefService* pref_service) : pref_service_(pref_service) {
  DCHECK(pref_service);
  // Watch the preference that indicates sync is managed so we can take
  // appropriate action.
  pref_sync_managed_.Init(
      prefs::kSyncManaged, pref_service_,
      base::BindRepeating(&SyncPrefs::OnSyncManagedPrefChanged,
                          base::Unretained(this)));
  pref_first_setup_complete_.Init(
      prefs::kSyncFirstSetupComplete, pref_service_,
      base::BindRepeating(&SyncPrefs::OnFirstSetupCompletePrefChange,
                          base::Unretained(this)));
  pref_sync_suppressed_.Init(
      prefs::kSyncSuppressStart, pref_service_,
      base::BindRepeating(&SyncPrefs::OnSyncSuppressedPrefChange,
                          base::Unretained(this)));

  // Cache the value of the kEnableLocalSyncBackend pref to avoid it flipping
  // during the lifetime of the service.
  local_sync_enabled_ =
      pref_service_->GetBoolean(prefs::kEnableLocalSyncBackend);
}

SyncPrefs::~SyncPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void SyncPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Actual user-controlled preferences.
  registry->RegisterBooleanPref(prefs::kSyncFirstSetupComplete, false);
  registry->RegisterBooleanPref(prefs::kSyncSuppressStart, false);
  registry->RegisterBooleanPref(prefs::kSyncKeepEverythingSynced, true);
  for (ModelType type : UserSelectableTypes()) {
    RegisterDataTypePreferredPref(registry, type);
  }

  // Internal or bookkeeping prefs.
  registry->RegisterStringPref(prefs::kSyncCacheGuid, std::string());
  registry->RegisterStringPref(prefs::kSyncBirthday, std::string());
  registry->RegisterStringPref(prefs::kSyncBagOfChips, std::string());
  registry->RegisterInt64Pref(prefs::kSyncLastSyncedTime, 0);
  registry->RegisterInt64Pref(prefs::kSyncLastPollTime, 0);
  registry->RegisterInt64Pref(prefs::kSyncPollIntervalSeconds, 0);
  registry->RegisterBooleanPref(prefs::kSyncManaged, false);
  registry->RegisterStringPref(prefs::kSyncEncryptionBootstrapToken,
                               std::string());
  registry->RegisterStringPref(prefs::kSyncKeystoreEncryptionBootstrapToken,
                               std::string());
#if defined(OS_CHROMEOS)
  // TODO(crbug.com/938869): Remove this pref.
  registry->RegisterStringPref(prefs::kSyncSpareBootstrapToken, "");
#endif
  registry->RegisterBooleanPref(prefs::kSyncPassphrasePrompted, false);
  registry->RegisterIntegerPref(prefs::kSyncMemoryPressureWarningCount, -1);
  registry->RegisterBooleanPref(prefs::kSyncShutdownCleanly, false);
  registry->RegisterDictionaryPref(prefs::kSyncInvalidationVersions);
  registry->RegisterStringPref(prefs::kSyncLastRunVersion, std::string());
  registry->RegisterBooleanPref(prefs::kEnableLocalSyncBackend, false);
  registry->RegisterFilePathPref(prefs::kLocalSyncBackendDir, base::FilePath());

  // Obsolete prefs that will be removed after a grace period.
  RegisterObsoleteUserTypePrefs(registry);
  registry->RegisterBooleanPref(kSyncPassphraseEncryptionTransitionInProgress,
                                false);
  registry->RegisterStringPref(kSyncNigoriStateForPassphraseTransition,
                               std::string());
  registry->RegisterBooleanPref(kSyncHasAuthError, false);
  registry->RegisterInt64Pref(kSyncFirstSyncTime, 0);
  registry->RegisterInt64Pref(kSyncLongPollIntervalSeconds, 0);
}

void SyncPrefs::AddSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.AddObserver(sync_pref_observer);
}

void SyncPrefs::RemoveSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.RemoveObserver(sync_pref_observer);
}

void SyncPrefs::ClearPreferences() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pref_service_->ClearPref(prefs::kSyncCacheGuid);
  pref_service_->ClearPref(prefs::kSyncBirthday);
  pref_service_->ClearPref(prefs::kSyncBagOfChips);
  pref_service_->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service_->ClearPref(prefs::kSyncLastPollTime);
  pref_service_->ClearPref(prefs::kSyncPollIntervalSeconds);
  pref_service_->ClearPref(prefs::kSyncEncryptionBootstrapToken);
  pref_service_->ClearPref(prefs::kSyncKeystoreEncryptionBootstrapToken);
  pref_service_->ClearPref(prefs::kSyncPassphrasePrompted);
  pref_service_->ClearPref(prefs::kSyncMemoryPressureWarningCount);
  pref_service_->ClearPref(prefs::kSyncShutdownCleanly);
  pref_service_->ClearPref(prefs::kSyncInvalidationVersions);
  pref_service_->ClearPref(prefs::kSyncLastRunVersion);
  // No need to clear kManaged, kEnableLocalSyncBackend or kLocalSyncBackendDir,
  // since they're never actually set as user preferences.

  // Note: We do *not* clear prefs which are directly user-controlled such as
  // the set of preferred data types here, so that if the user ever chooses to
  // enable Sync again, they start off with their previous settings by default.
  // We do however require going through first-time setup again.
  pref_service_->ClearPref(prefs::kSyncFirstSetupComplete);
}

bool SyncPrefs::IsFirstSetupComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncFirstSetupComplete);
}

void SyncPrefs::SetFirstSetupComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncFirstSetupComplete, true);
}

bool SyncPrefs::IsSyncRequested() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // IsSyncRequested is the inverse of the old SuppressStart pref.
  // Since renaming a pref value is hard, here we still use the old one.
  return !pref_service_->GetBoolean(prefs::kSyncSuppressStart);
}

void SyncPrefs::SetSyncRequested(bool is_requested) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // See IsSyncRequested for why we use this pref and !is_requested.
  pref_service_->SetBoolean(prefs::kSyncSuppressStart, !is_requested);
}

base::Time SyncPrefs::GetLastSyncedTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncLastSyncedTime));
}

void SyncPrefs::SetLastSyncedTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(prefs::kSyncLastSyncedTime, time.ToInternalValue());
}

base::Time SyncPrefs::GetLastPollTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncLastPollTime));
}

void SyncPrefs::SetLastPollTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(prefs::kSyncLastPollTime, time.ToInternalValue());
}

base::TimeDelta SyncPrefs::GetPollInterval() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::TimeDelta::FromSeconds(
      pref_service_->GetInt64(prefs::kSyncPollIntervalSeconds));
}

void SyncPrefs::SetPollInterval(base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(prefs::kSyncPollIntervalSeconds,
                          interval.InSeconds());
}

bool SyncPrefs::HasKeepEverythingSynced() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced);
}

ModelTypeSet SyncPrefs::GetPreferredDataTypes(
    ModelTypeSet registered_types) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced)) {
    return registered_types;
  }

  ModelTypeSet preferred_types = AlwaysPreferredUserTypes();

  for (ModelType type : Intersection(UserSelectableTypes(), registered_types)) {
    if (IsDataTypeChosen(type)) {
      preferred_types.Put(type);
    }
  }

  preferred_types = ResolvePrefGroups(preferred_types);
  preferred_types.RetainAll(registered_types);
  return preferred_types;
}

void SyncPrefs::SetDataTypesConfiguration(bool keep_everything_synced,
                                          ModelTypeSet registered_types,
                                          ModelTypeSet chosen_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(UserSelectableTypes().HasAll(chosen_types));
  // TODO(crbug.com/906611): remove |registered_types| parameter. It have no
  // real usage and needed only to control the absence of behavioral changes
  // during the landing of the patch. Also consider removal of this parameter
  // from the getter.
  DCHECK(registered_types.HasAll(chosen_types));

  pref_service_->SetBoolean(prefs::kSyncKeepEverythingSynced,
                            keep_everything_synced);

  for (ModelType type : Intersection(UserSelectableTypes(), registered_types)) {
    SetDataTypeChosen(type, chosen_types.Has(type));
  }

  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange();
  }
}

bool SyncPrefs::IsManaged() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncManaged);
}

std::string SyncPrefs::GetEncryptionBootstrapToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(prefs::kSyncEncryptionBootstrapToken);
}

void SyncPrefs::SetEncryptionBootstrapToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(prefs::kSyncEncryptionBootstrapToken, token);
}

std::string SyncPrefs::GetKeystoreEncryptionBootstrapToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(prefs::kSyncKeystoreEncryptionBootstrapToken);
}

void SyncPrefs::SetKeystoreEncryptionBootstrapToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(prefs::kSyncKeystoreEncryptionBootstrapToken, token);
}

// static
const char* SyncPrefs::GetPrefNameForDataType(ModelType type) {
  switch (type) {
    case UNSPECIFIED:
    case TOP_LEVEL_FOLDER:
    case AUTOFILL_PROFILE:
    case AUTOFILL_WALLET_DATA:
    case AUTOFILL_WALLET_METADATA:
    case SEARCH_ENGINES:
    case APP_SETTINGS:
    case EXTENSION_SETTINGS:
    case DEPRECATED_APP_NOTIFICATIONS:
    case HISTORY_DELETE_DIRECTIVES:
    case DEPRECATED_SYNCED_NOTIFICATIONS:
    case DEPRECATED_SYNCED_NOTIFICATION_APP_INFO:
    case DICTIONARY:
    case FAVICON_IMAGES:
    case FAVICON_TRACKING:
    case DEVICE_INFO:
    case PRIORITY_PREFERENCES:
    case SUPERVISED_USER_SETTINGS:
    case DEPRECATED_SUPERVISED_USERS:
    case DEPRECATED_SUPERVISED_USER_SHARED_SETTINGS:
    case DEPRECATED_ARTICLES:
    case APP_LIST:
    case DEPRECATED_WIFI_CREDENTIALS:
    case SUPERVISED_USER_WHITELISTS:
    case ARC_PACKAGE:
    case PRINTERS:
    case USER_EVENTS:
    case SECURITY_EVENTS:
    case MOUNTAIN_SHARES:
    case USER_CONSENTS:
    case SEND_TAB_TO_SELF:
    case NIGORI:
    case DEPRECATED_EXPERIMENTS:
    case MODEL_TYPE_COUNT:
    case SESSIONS:
      break;
    case BOOKMARKS:
      return prefs::kSyncBookmarks;
    case PREFERENCES:
      return prefs::kSyncPreferences;
    case PASSWORDS:
      return prefs::kSyncPasswords;
    case AUTOFILL:
      return prefs::kSyncAutofill;
    case THEMES:
      return prefs::kSyncThemes;
    case TYPED_URLS:
      return prefs::kSyncTypedUrls;
    case EXTENSIONS:
      return prefs::kSyncExtensions;
    case APPS:
      return prefs::kSyncApps;
    case READING_LIST:
      return prefs::kSyncReadingList;
    case PROXY_TABS:
      return prefs::kSyncTabs;
  }
  NOTREACHED() << "No pref mapping for type " << ModelTypeToString(type);
  return nullptr;
}

#if defined(OS_CHROMEOS)
std::string SyncPrefs::GetSpareBootstrapToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(prefs::kSyncSpareBootstrapToken);
}

void SyncPrefs::SetSpareBootstrapToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(prefs::kSyncSpareBootstrapToken, token);
}
#endif

void SyncPrefs::OnSyncManagedPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_)
    observer.OnSyncManagedPrefChange(*pref_sync_managed_);
}

void SyncPrefs::OnFirstSetupCompletePrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_)
    observer.OnFirstSetupCompletePrefChange(*pref_first_setup_complete_);
}

void SyncPrefs::OnSyncSuppressedPrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Note: The pref is inverted for historic reasons; see IsSyncRequested.
  for (SyncPrefObserver& observer : sync_pref_observers_)
    observer.OnSyncRequestedPrefChange(!*pref_sync_suppressed_);
}

void SyncPrefs::SetManagedForTest(bool is_managed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncManaged, is_managed);
}

// static
void SyncPrefs::RegisterDataTypePreferredPref(
    user_prefs::PrefRegistrySyncable* registry,
    ModelType type) {
  const char* pref_name = GetPrefNameForDataType(type);
  DCHECK(pref_name);
  registry->RegisterBooleanPref(pref_name, false);
}

bool SyncPrefs::IsDataTypeChosen(ModelType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const char* pref_name = GetPrefNameForDataType(type);
  DCHECK(pref_name);
  DCHECK(IsUserSelectableType(type));

  return pref_service_->GetBoolean(pref_name);
}

void SyncPrefs::SetDataTypeChosen(ModelType type, bool is_chosen) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const char* pref_name = GetPrefNameForDataType(type);
  DCHECK(pref_name);
  DCHECK(IsUserSelectableType(type));

  pref_service_->SetBoolean(pref_name, is_chosen);
}

// static
ModelTypeSet SyncPrefs::ResolvePrefGroups(ModelTypeSet types) {
  ModelTypeSet types_with_groups = types;
  for (const auto& pref_group : ComputePrefGroups()) {
    if (types.Has(pref_group.first)) {
      types_with_groups.PutAll(pref_group.second);
    }
  }
  return types_with_groups;
}

void SyncPrefs::SetCacheGuid(const std::string& cache_guid) {
  pref_service_->SetString(prefs::kSyncCacheGuid, cache_guid);
}

std::string SyncPrefs::GetCacheGuid() const {
  return pref_service_->GetString(prefs::kSyncCacheGuid);
}

void SyncPrefs::SetBirthday(const std::string& birthday) {
  pref_service_->SetString(prefs::kSyncBirthday, birthday);
}

std::string SyncPrefs::GetBirthday() const {
  return pref_service_->GetString(prefs::kSyncBirthday);
}

void SyncPrefs::SetBagOfChips(const std::string& bag_of_chips) {
  // |bag_of_chips| contains a serialized proto which is not utf-8, hence we use
  // base64 encoding in prefs.
  std::string encoded;
  base::Base64Encode(bag_of_chips, &encoded);
  pref_service_->SetString(prefs::kSyncBagOfChips, encoded);
}

std::string SyncPrefs::GetBagOfChips() const {
  // |kSyncBagOfChips| gets stored in base64 because it represents a serialized
  // proto which is not utf-8 encoding.
  const std::string encoded = pref_service_->GetString(prefs::kSyncBagOfChips);
  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  return decoded;
}

bool SyncPrefs::IsPassphrasePrompted() const {
  return pref_service_->GetBoolean(prefs::kSyncPassphrasePrompted);
}

void SyncPrefs::SetPassphrasePrompted(bool value) {
  pref_service_->SetBoolean(prefs::kSyncPassphrasePrompted, value);
}

int SyncPrefs::GetMemoryPressureWarningCount() const {
  return pref_service_->GetInteger(prefs::kSyncMemoryPressureWarningCount);
}

void SyncPrefs::SetMemoryPressureWarningCount(int value) {
  pref_service_->SetInteger(prefs::kSyncMemoryPressureWarningCount, value);
}

bool SyncPrefs::DidSyncShutdownCleanly() const {
  return pref_service_->GetBoolean(prefs::kSyncShutdownCleanly);
}

void SyncPrefs::SetCleanShutdown(bool value) {
  pref_service_->SetBoolean(prefs::kSyncShutdownCleanly, value);
}

void SyncPrefs::GetInvalidationVersions(
    std::map<ModelType, int64_t>* invalidation_versions) const {
  const base::DictionaryValue* invalidation_dictionary =
      pref_service_->GetDictionary(prefs::kSyncInvalidationVersions);
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType type : protocol_types) {
    std::string key = ModelTypeToString(type);
    std::string version_str;
    if (!invalidation_dictionary->GetString(key, &version_str))
      continue;
    int64_t version = 0;
    if (!base::StringToInt64(version_str, &version))
      continue;
    (*invalidation_versions)[type] = version;
  }
}

void SyncPrefs::UpdateInvalidationVersions(
    const std::map<ModelType, int64_t>& invalidation_versions) {
  std::unique_ptr<base::DictionaryValue> invalidation_dictionary(
      new base::DictionaryValue());
  for (const auto& map_iter : invalidation_versions) {
    std::string version_str = base::NumberToString(map_iter.second);
    invalidation_dictionary->SetString(ModelTypeToString(map_iter.first),
                                       version_str);
  }
  pref_service_->Set(prefs::kSyncInvalidationVersions,
                     *invalidation_dictionary);
}

std::string SyncPrefs::GetLastRunVersion() const {
  return pref_service_->GetString(prefs::kSyncLastRunVersion);
}

void SyncPrefs::SetLastRunVersion(const std::string& current_version) {
  pref_service_->SetString(prefs::kSyncLastRunVersion, current_version);
}

bool SyncPrefs::IsLocalSyncEnabled() const {
  return local_sync_enabled_;
}

void MigrateSessionsToProxyTabsPrefs(PrefService* pref_service) {
  if (pref_service->GetUserPrefValue(prefs::kSyncTabs) == nullptr &&
      pref_service->GetUserPrefValue(prefs::kSyncSessions) != nullptr &&
      pref_service->IsUserModifiablePreference(prefs::kSyncTabs)) {
    // If there is no tab sync preference yet (i.e. newly enabled type),
    // default to the session sync preference value.
    bool sessions_pref_value = pref_service->GetBoolean(prefs::kSyncSessions);
    pref_service->SetBoolean(prefs::kSyncTabs, sessions_pref_value);
  }
}

void ClearObsoleteUserTypePrefs(PrefService* pref_service) {
  for (const std::string& obsolete_pref : GetObsoleteUserTypePrefs()) {
    pref_service->ClearPref(obsolete_pref);
  }
}

void ClearObsoleteClearServerDataPrefs(PrefService* pref_service) {
  pref_service->ClearPref(kSyncPassphraseEncryptionTransitionInProgress);
  pref_service->ClearPref(kSyncNigoriStateForPassphraseTransition);
}

void ClearObsoleteAuthErrorPrefs(PrefService* pref_service) {
  pref_service->ClearPref(kSyncHasAuthError);
}

void ClearObsoleteFirstSyncTime(PrefService* pref_service) {
  pref_service->ClearPref(kSyncFirstSyncTime);
}

void ClearObsoleteSyncLongPollIntervalSeconds(PrefService* pref_service) {
  pref_service->ClearPref(kSyncLongPollIntervalSeconds);
}

}  // namespace syncer
