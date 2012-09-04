// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/invalidations/invalidator_storage.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "sync/internal_api/public/base/model_type.h"

using syncer::InvalidationVersionMap;

namespace browser_sync {

namespace {

const char kSourceKey[] = "source";
const char kNameKey[] = "name";
const char kMaxVersionKey[] = "max-version";

bool ValueToObjectIdAndVersion(const DictionaryValue& value,
                               invalidation::ObjectId* id,
                               int64* max_version) {
  std::string source_str;
  int source = 0;
  std::string name;
  std::string max_version_str;
  if (!value.GetString(kSourceKey, &source_str)) {
    DLOG(WARNING) << "Unable to deserialize source";
    return false;
  }
  if (!value.GetString(kNameKey, &name)) {
    DLOG(WARNING) << "Unable to deserialize name";
    return false;
  }
  if (!value.GetString(kMaxVersionKey, &max_version_str)) {
    DLOG(WARNING) << "Unable to deserialize max version";
    return false;
  }
  if (!base::StringToInt(source_str, &source)) {
    DLOG(WARNING) << "Invalid source: " << source_str;
    return false;
  }
  if (!base::StringToInt64(max_version_str, max_version)) {
    DLOG(WARNING) << "Invalid max invalidation version: " << max_version_str;
    return false;
  }
  *id = invalidation::ObjectId(source, name);
  return true;
}

// The caller owns the returned value.
DictionaryValue* ObjectIdAndVersionToValue(const invalidation::ObjectId& id,
                                           int64 max_version) {
  DictionaryValue* value = new DictionaryValue;
  value->SetString(kSourceKey, base::IntToString(id.source()));
  value->SetString(kNameKey, id.name());
  value->SetString(kMaxVersionKey, base::Int64ToString(max_version));
  return value;
}

}  // namespace

InvalidatorStorage::InvalidatorStorage(PrefService* pref_service)
    : pref_service_(pref_service) {
  // TODO(tim): Create a Mock instead of maintaining the if(!pref_service_) case
  // throughout this file.  This is a problem now due to lack of injection at
  // ProfileSyncService. Bug 130176.
  if (pref_service_) {
    pref_service_->RegisterListPref(prefs::kInvalidatorMaxInvalidationVersions,
                                    PrefService::UNSYNCABLE_PREF);
    pref_service_->RegisterStringPref(prefs::kInvalidatorInvalidationState,
                                      std::string(),
                                      PrefService::UNSYNCABLE_PREF);

    MigrateMaxInvalidationVersionsPref();
  }
}

InvalidatorStorage::~InvalidatorStorage() {
}

InvalidationVersionMap InvalidatorStorage::GetAllMaxVersions() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  InvalidationVersionMap max_versions;
  if (!pref_service_) {
    return max_versions;
  }
  const base::ListValue* max_versions_list =
      pref_service_->GetList(prefs::kInvalidatorMaxInvalidationVersions);
  CHECK(max_versions_list);
  DeserializeFromList(*max_versions_list, &max_versions);
  return max_versions;
}

void InvalidatorStorage::SetMaxVersion(const invalidation::ObjectId& id,
                                       int64 max_version) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(pref_service_);
  InvalidationVersionMap max_versions = GetAllMaxVersions();
  InvalidationVersionMap::iterator it = max_versions.find(id);
  if ((it != max_versions.end()) && (max_version <= it->second)) {
    NOTREACHED();
    return;
  }
  max_versions[id] = max_version;

  base::ListValue max_versions_list;
  SerializeToList(max_versions, &max_versions_list);
  pref_service_->Set(prefs::kInvalidatorMaxInvalidationVersions,
                     max_versions_list);
}

void InvalidatorStorage::Forget(const syncer::ObjectIdSet& ids) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(pref_service_);
  InvalidationVersionMap max_versions = GetAllMaxVersions();
  for (syncer::ObjectIdSet::const_iterator it = ids.begin(); it != ids.end();
       ++it) {
    max_versions.erase(*it);
  }

  base::ListValue max_versions_list;
  SerializeToList(max_versions, &max_versions_list);
  pref_service_->Set(prefs::kInvalidatorMaxInvalidationVersions,
                     max_versions_list);
}

// static
void InvalidatorStorage::DeserializeFromList(
    const base::ListValue& max_versions_list,
    InvalidationVersionMap* max_versions_map) {
  max_versions_map->clear();
  for (size_t i = 0; i < max_versions_list.GetSize(); ++i) {
    const DictionaryValue* value = NULL;
    if (!max_versions_list.GetDictionary(i, &value)) {
      DLOG(WARNING) << "Unable to deserialize entry " << i;
      continue;
    }
    invalidation::ObjectId id;
    int64 max_version = 0;
    if (!ValueToObjectIdAndVersion(*value, &id, &max_version)) {
      DLOG(WARNING) << "Error while deserializing entry " << i;
      continue;
    }
    (*max_versions_map)[id] = max_version;
  }
}

// static
void InvalidatorStorage::SerializeToList(
    const InvalidationVersionMap& max_versions_map,
    base::ListValue* max_versions_list) {
  for (InvalidationVersionMap::const_iterator it = max_versions_map.begin();
       it != max_versions_map.end(); ++it) {
    max_versions_list->Append(ObjectIdAndVersionToValue(it->first, it->second));
  }
}

// Legacy migration code.
void InvalidatorStorage::MigrateMaxInvalidationVersionsPref() {
  pref_service_->RegisterDictionaryPref(prefs::kSyncMaxInvalidationVersions,
                                        PrefService::UNSYNCABLE_PREF);
  const base::DictionaryValue* max_versions_dict =
      pref_service_->GetDictionary(prefs::kSyncMaxInvalidationVersions);
  CHECK(max_versions_dict);
  if (!max_versions_dict->empty()) {
    InvalidationVersionMap max_versions;
    DeserializeMap(max_versions_dict, &max_versions);
    base::ListValue max_versions_list;
    SerializeToList(max_versions, &max_versions_list);
    pref_service_->Set(prefs::kInvalidatorMaxInvalidationVersions,
                       max_versions_list);
    UMA_HISTOGRAM_BOOLEAN("InvalidatorStorage.MigrateInvalidationVersionsPref",
                          true);
  } else {
    UMA_HISTOGRAM_BOOLEAN("InvalidatorStorage.MigrateInvalidationVersionsPref",
                          false);
  }
  pref_service_->ClearPref(prefs::kSyncMaxInvalidationVersions);
}

// Legacy migration code.
// static
void InvalidatorStorage::DeserializeMap(
    const base::DictionaryValue* max_versions_dict,
    InvalidationVersionMap* map) {
  map->clear();
  // Convert from a string -> string DictionaryValue to a
  // ModelType -> int64 map.
  for (base::DictionaryValue::key_iterator it =
           max_versions_dict->begin_keys();
       it != max_versions_dict->end_keys(); ++it) {
    int model_type_int = 0;
    if (!base::StringToInt(*it, &model_type_int)) {
      LOG(WARNING) << "Invalid model type key: " << *it;
      continue;
    }
    if ((model_type_int < syncer::FIRST_REAL_MODEL_TYPE) ||
        (model_type_int >= syncer::MODEL_TYPE_COUNT)) {
      LOG(WARNING) << "Out-of-range model type key: " << model_type_int;
      continue;
    }
    const syncer::ModelType model_type =
        syncer::ModelTypeFromInt(model_type_int);
    std::string max_version_str;
    CHECK(max_versions_dict->GetString(*it, &max_version_str));
    int64 max_version = 0;
    if (!base::StringToInt64(max_version_str, &max_version)) {
      LOG(WARNING) << "Invalid max invalidation version for "
                   << syncer::ModelTypeToString(model_type) << ": "
                   << max_version_str;
      continue;
    }
    invalidation::ObjectId id;
    if (!syncer::RealModelTypeToObjectId(model_type, &id)) {
      DLOG(WARNING) << "Invalid model type: " << model_type;
      continue;
    }
    (*map)[id] = max_version;
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
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string utf8_state;
  base::Base64Encode(state, &utf8_state);
  pref_service_->SetString(prefs::kInvalidatorInvalidationState,
                           utf8_state);
}

void InvalidatorStorage::Clear() {
  DCHECK(thread_checker_.CalledOnValidThread());
  pref_service_->ClearPref(prefs::kInvalidatorMaxInvalidationVersions);
  pref_service_->ClearPref(prefs::kInvalidatorInvalidationState);
}

}  // namespace browser_sync
