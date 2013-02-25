// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/invalidations/invalidator_storage.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/common/pref_names.h"
#include "sync/internal_api/public/base/model_type.h"

using syncer::InvalidationStateMap;

namespace browser_sync {

namespace {

const char kSourceKey[] = "source";
const char kNameKey[] = "name";
const char kMaxVersionKey[] = "max-version";
const char kPayloadKey[] = "payload";
const char kCurrentAckHandleKey[] = "current-ack";
const char kExpectedAckHandleKey[] = "expected-ack";

bool ValueToObjectIdAndState(const DictionaryValue& value,
                             invalidation::ObjectId* id,
                             syncer::InvalidationState* state) {
  std::string source_str;
  if (!value.GetString(kSourceKey, &source_str)) {
    DLOG(WARNING) << "Unable to deserialize source";
    return false;
  }
  int source = 0;
  if (!base::StringToInt(source_str, &source)) {
    DLOG(WARNING) << "Invalid source: " << source_str;
    return false;
  }
  std::string name;
  if (!value.GetString(kNameKey, &name)) {
    DLOG(WARNING) << "Unable to deserialize name";
    return false;
  }
  *id = invalidation::ObjectId(source, name);
  std::string max_version_str;
  if (!value.GetString(kMaxVersionKey, &max_version_str)) {
    DLOG(WARNING) << "Unable to deserialize max version";
    return false;
  }
  if (!base::StringToInt64(max_version_str, &state->version)) {
    DLOG(WARNING) << "Invalid max invalidation version: " << max_version_str;
    return false;
  }
  value.GetString(kPayloadKey, &state->payload);
  // The ack handle fields won't be set if upgrading from previous versions of
  // Chrome.
  const base::DictionaryValue* current_ack_handle_value = NULL;
  if (value.GetDictionary(kCurrentAckHandleKey, &current_ack_handle_value)) {
    state->current.ResetFromValue(*current_ack_handle_value);
  }
  const base::DictionaryValue* expected_ack_handle_value = NULL;
  if (value.GetDictionary(kExpectedAckHandleKey, &expected_ack_handle_value)) {
    state->expected.ResetFromValue(*expected_ack_handle_value);
  } else {
    // In this case, we should never have a valid current value set.
    DCHECK(!state->current.IsValid());
    state->current = syncer::AckHandle::InvalidAckHandle();
  }
  return true;
}

// The caller owns the returned value.
DictionaryValue* ObjectIdAndStateToValue(
    const invalidation::ObjectId& id, const syncer::InvalidationState& state) {
  DictionaryValue* value = new DictionaryValue;
  value->SetString(kSourceKey, base::IntToString(id.source()));
  value->SetString(kNameKey, id.name());
  value->SetString(kMaxVersionKey, base::Int64ToString(state.version));
  value->SetString(kPayloadKey, state.payload);
  if (state.current.IsValid())
    value->Set(kCurrentAckHandleKey, state.current.ToValue().release());
  if (state.expected.IsValid())
    value->Set(kExpectedAckHandleKey, state.expected.ToValue().release());
  return value;
}

}  // namespace

// static
void InvalidatorStorage::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kInvalidatorMaxInvalidationVersions,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kInvalidatorInvalidationState,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kInvalidatorClientId,
                                 std::string(),
                                 PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kSyncMaxInvalidationVersions,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
}

InvalidatorStorage::InvalidatorStorage(PrefService* pref_service)
    : pref_service_(pref_service) {
  // TODO(tim): Create a Mock instead of maintaining the if(!pref_service_) case
  // throughout this file.  This is a problem now due to lack of injection at
  // ProfileSyncService. Bug 130176.
  if (pref_service_)
    MigrateMaxInvalidationVersionsPref();
}

InvalidatorStorage::~InvalidatorStorage() {
}

InvalidationStateMap InvalidatorStorage::GetAllInvalidationStates() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  InvalidationStateMap state_map;
  if (!pref_service_) {
    return state_map;
  }
  const base::ListValue* state_map_list =
      pref_service_->GetList(prefs::kInvalidatorMaxInvalidationVersions);
  CHECK(state_map_list);
  DeserializeFromList(*state_map_list, &state_map);
  return state_map;
}

void InvalidatorStorage::SetMaxVersionAndPayload(
    const invalidation::ObjectId& id,
    int64 max_version,
    const std::string& payload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(pref_service_);
  InvalidationStateMap state_map = GetAllInvalidationStates();
  InvalidationStateMap::iterator it = state_map.find(id);
  if ((it != state_map.end()) && (max_version <= it->second.version)) {
    NOTREACHED();
    return;
  }
  state_map[id].version = max_version;
  state_map[id].payload = payload;

  base::ListValue state_map_list;
  SerializeToList(state_map, &state_map_list);
  pref_service_->Set(prefs::kInvalidatorMaxInvalidationVersions,
                     state_map_list);
}

void InvalidatorStorage::Forget(const syncer::ObjectIdSet& ids) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(pref_service_);
  InvalidationStateMap state_map = GetAllInvalidationStates();
  for (syncer::ObjectIdSet::const_iterator it = ids.begin(); it != ids.end();
       ++it) {
    state_map.erase(*it);
  }

  base::ListValue state_map_list;
  SerializeToList(state_map, &state_map_list);
  pref_service_->Set(prefs::kInvalidatorMaxInvalidationVersions,
                     state_map_list);
}

// static
void InvalidatorStorage::DeserializeFromList(
    const base::ListValue& state_map_list,
    InvalidationStateMap* state_map) {
  state_map->clear();
  for (size_t i = 0; i < state_map_list.GetSize(); ++i) {
    const DictionaryValue* value = NULL;
    if (!state_map_list.GetDictionary(i, &value)) {
      DLOG(WARNING) << "Unable to deserialize entry " << i;
      continue;
    }
    invalidation::ObjectId id;
    syncer::InvalidationState state;
    if (!ValueToObjectIdAndState(*value, &id, &state)) {
      DLOG(WARNING) << "Error while deserializing entry " << i;
      continue;
    }
    (*state_map)[id] = state;
  }
}

// static
void InvalidatorStorage::SerializeToList(
    const InvalidationStateMap& state_map,
    base::ListValue* state_map_list) {
  for (InvalidationStateMap::const_iterator it = state_map.begin();
       it != state_map.end(); ++it) {
    state_map_list->Append(ObjectIdAndStateToValue(it->first, it->second));
  }
}

// Legacy migration code.
void InvalidatorStorage::MigrateMaxInvalidationVersionsPref() {
  const base::DictionaryValue* max_versions_dict =
      pref_service_->GetDictionary(prefs::kSyncMaxInvalidationVersions);
  CHECK(max_versions_dict);
  if (!max_versions_dict->empty()) {
    InvalidationStateMap state_map;
    DeserializeMap(max_versions_dict, &state_map);
    base::ListValue state_map_list;
    SerializeToList(state_map, &state_map_list);
    pref_service_->Set(prefs::kInvalidatorMaxInvalidationVersions,
                       state_map_list);
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
    InvalidationStateMap* map) {
  map->clear();
  // Convert from a string -> string DictionaryValue to a
  // ModelType -> int64 map.
  for (base::DictionaryValue::Iterator it(*max_versions_dict); !it.IsAtEnd();
       it.Advance()) {
    int model_type_int = 0;
    if (!base::StringToInt(it.key(), &model_type_int)) {
      LOG(WARNING) << "Invalid model type key: " << it.key();
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
    CHECK(it.value().GetAsString(&max_version_str));
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
    (*map)[id].version = max_version;
  }
}

void InvalidatorStorage::SetInvalidatorClientId(const std::string& client_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  pref_service_->SetString(prefs::kInvalidatorClientId, client_id);
}

std::string InvalidatorStorage::GetInvalidatorClientId() const {
  return pref_service_ ?
      pref_service_->GetString(prefs::kInvalidatorClientId) :
          std::string();
}

void InvalidatorStorage::SetBootstrapData(const std::string& data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string base64_data;
  base::Base64Encode(data, &base64_data);
  pref_service_->SetString(prefs::kInvalidatorInvalidationState,
                           base64_data);
}

std::string InvalidatorStorage::GetBootstrapData() const {
  std::string base64_data(pref_service_ ?
      pref_service_->GetString(prefs::kInvalidatorInvalidationState) : "");
  std::string data;
  base::Base64Decode(base64_data, &data);
  return data;
}

void InvalidatorStorage::Clear() {
  DCHECK(thread_checker_.CalledOnValidThread());
  pref_service_->ClearPref(prefs::kInvalidatorMaxInvalidationVersions);
  pref_service_->ClearPref(prefs::kInvalidatorClientId);
  pref_service_->ClearPref(prefs::kInvalidatorInvalidationState);
}

void InvalidatorStorage::GenerateAckHandles(
    const syncer::ObjectIdSet& ids,
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Callback<void(const syncer::AckHandleMap&)> callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(pref_service_);
  InvalidationStateMap state_map = GetAllInvalidationStates();

  syncer::AckHandleMap ack_handles;
  for (syncer::ObjectIdSet::const_iterator it = ids.begin(); it != ids.end();
       ++it) {
    state_map[*it].expected = syncer::AckHandle::CreateUnique();
    ack_handles.insert(std::make_pair(*it, state_map[*it].expected));
  }

  base::ListValue state_map_list;
  SerializeToList(state_map, &state_map_list);
  pref_service_->Set(prefs::kInvalidatorMaxInvalidationVersions,
                     state_map_list);

  ignore_result(task_runner->PostTask(FROM_HERE,
                                      base::Bind(callback, ack_handles)));
}

void InvalidatorStorage::Acknowledge(const invalidation::ObjectId& id,
                                     const syncer::AckHandle& ack_handle) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(pref_service_);
  InvalidationStateMap state_map = GetAllInvalidationStates();

  InvalidationStateMap::iterator it = state_map.find(id);
  // This could happen if the acknowledgement is delayed and Forget() has
  // already been called.
  if (it == state_map.end())
    return;
  it->second.current = ack_handle;

  base::ListValue state_map_list;
  SerializeToList(state_map, &state_map_list);
  pref_service_->Set(prefs::kInvalidatorMaxInvalidationVersions,
                     state_map_list);
}

}  // namespace browser_sync
