// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/invalidations/invalidator_storage.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

using csync::InvalidationVersionMap;

namespace browser_sync {

InvalidatorStorage::InvalidatorStorage(PrefService* pref_service)
    : pref_service_(pref_service) {
  // TODO(tim): Create a Mock instead of maintaining the if(!pref_service_) case
  // throughout this file.  This is a problem now due to lack of injection at
  // ProfileSyncService. Bug 130176.
  if (pref_service_) {
    pref_service_->RegisterDictionaryPref(prefs::kSyncMaxInvalidationVersions,
                                          PrefService::UNSYNCABLE_PREF);
    pref_service_->RegisterStringPref(prefs::kInvalidatorInvalidationState,
                                      std::string(),
                                      PrefService::UNSYNCABLE_PREF);
  }
}

InvalidatorStorage::~InvalidatorStorage() {
}

InvalidationVersionMap InvalidatorStorage::GetAllMaxVersions() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!pref_service_) {
    return InvalidationVersionMap();
  }

  const base::DictionaryValue* max_versions_dict =
      pref_service_->GetDictionary(prefs::kSyncMaxInvalidationVersions);
  CHECK(max_versions_dict);
  InvalidationVersionMap max_versions;
  DeserializeMap(max_versions_dict, &max_versions);
  return max_versions;
}

void InvalidatorStorage::SetMaxVersion(syncable::ModelType model_type,
                                       int64 max_version) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(syncable::IsRealDataType(model_type));
  CHECK(pref_service_);
  InvalidationVersionMap max_versions =
      GetAllMaxVersions();
  InvalidationVersionMap::iterator it =
      max_versions.find(model_type);
  if ((it != max_versions.end()) && (max_version <= it->second)) {
    NOTREACHED();
    return;
  }
  max_versions[model_type] = max_version;

  base::DictionaryValue max_versions_dict;
  SerializeMap(max_versions, &max_versions_dict);
  pref_service_->Set(prefs::kSyncMaxInvalidationVersions, max_versions_dict);
}

// static
void InvalidatorStorage::DeserializeMap(
    const base::DictionaryValue* max_versions_dict,
    InvalidationVersionMap* map) {
  map->clear();
  // Convert from a string -> string DictionaryValue to a
  // ModelType -> int64 map
  // .
  for (base::DictionaryValue::key_iterator it =
           max_versions_dict->begin_keys();
       it != max_versions_dict->end_keys(); ++it) {
    int model_type_int = 0;
    if (!base::StringToInt(*it, &model_type_int)) {
      LOG(WARNING) << "Invalid model type key: " << *it;
      continue;
    }
    if ((model_type_int < syncable::FIRST_REAL_MODEL_TYPE) ||
        (model_type_int >= syncable::MODEL_TYPE_COUNT)) {
      LOG(WARNING) << "Out-of-range model type key: " << model_type_int;
      continue;
    }
    const syncable::ModelType model_type =
        syncable::ModelTypeFromInt(model_type_int);
    std::string max_version_str;
    CHECK(max_versions_dict->GetString(*it, &max_version_str));
    int64 max_version = 0;
    if (!base::StringToInt64(max_version_str, &max_version)) {
      LOG(WARNING) << "Invalid max invalidation version for "
                   << syncable::ModelTypeToString(model_type) << ": "
                   << max_version_str;
      continue;
    }
    (*map)[model_type] = max_version;
  }
}

// static
void InvalidatorStorage::SerializeMap(
    const InvalidationVersionMap& map, base::DictionaryValue* to_dict) {
  // Convert from a ModelType -> int64 map to a string -> string
  // DictionaryValue.
  for (InvalidationVersionMap::const_iterator it = map.begin();
       it != map.end(); ++it) {
    to_dict->SetString(
        base::IntToString(it->first),
        base::Int64ToString(it->second));
  }
}

std::string InvalidatorStorage::GetInvalidationState() const {
  std::string utf8_state(pref_service_ ?
      pref_service_->GetString(prefs::kInvalidatorInvalidationState) : "");
  std::string state_data;
  base::Base64Decode(utf8_state, &state_data);
  return state_data;
}

void InvalidatorStorage::SetInvalidationState(const std::string& state) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  std::string utf8_state;
  base::Base64Encode(state, &utf8_state);
  pref_service_->SetString(prefs::kInvalidatorInvalidationState,
                           utf8_state);
}

void InvalidatorStorage::Clear() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  pref_service_->ClearPref(prefs::kInvalidatorInvalidationState);
  pref_service_->ClearPref(prefs::kSyncMaxInvalidationVersions);
}

}  // namespace browser_sync
