// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/value_conversions.h"
#include "build/build_config.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_value_store.h"

namespace {

class ReadErrorHandler : public PersistentPrefStore::ReadErrorDelegate {
 public:
  using ErrorCallback =
      base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>;
  explicit ReadErrorHandler(ErrorCallback cb) : callback_(cb) {}

  void OnError(PersistentPrefStore::PrefReadError error) override {
    callback_.Run(error);
  }

 private:
  ErrorCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ReadErrorHandler);
};

// Returns the WriteablePrefStore::PrefWriteFlags for the pref with the given
// |path|.
uint32_t GetWriteFlags(const PrefService::Preference* pref) {
  uint32_t write_flags = WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS;

  if (!pref)
    return write_flags;

  if (pref->registration_flags() & PrefRegistry::LOSSY_PREF)
    write_flags |= WriteablePrefStore::LOSSY_PREF_WRITE_FLAG;
  return write_flags;
}

}  // namespace

PrefService::PrefService(
    std::unique_ptr<PrefNotifierImpl> pref_notifier,
    std::unique_ptr<PrefValueStore> pref_value_store,
    scoped_refptr<PersistentPrefStore> user_prefs,
    scoped_refptr<PrefRegistry> pref_registry,
    base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
        read_error_callback,
    bool async)
    : pref_notifier_(std::move(pref_notifier)),
      pref_value_store_(std::move(pref_value_store)),
      user_pref_store_(std::move(user_prefs)),
      read_error_callback_(std::move(read_error_callback)),
      pref_registry_(std::move(pref_registry)) {
  pref_notifier_->SetPrefService(this);

  DCHECK(pref_registry_);
  DCHECK(pref_value_store_);

  InitFromStorage(async);
}

PrefService::~PrefService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PrefService::InitFromStorage(bool async) {
  if (user_pref_store_->IsInitializationComplete()) {
    read_error_callback_.Run(user_pref_store_->GetReadError());
  } else if (!async) {
    read_error_callback_.Run(user_pref_store_->ReadPrefs());
  } else {
    // Guarantee that initialization happens after this function returned.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&PersistentPrefStore::ReadPrefsAsync, user_pref_store_,
                   new ReadErrorHandler(read_error_callback_)));
  }
}

void PrefService::CommitPendingWrite() {
  CommitPendingWrite(base::OnceClosure());
}

void PrefService::CommitPendingWrite(base::OnceClosure done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_pref_store_->CommitPendingWrite(std::move(done_callback));
}

void PrefService::SchedulePendingLossyWrites() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_pref_store_->SchedulePendingLossyWrites();
}

bool PrefService::GetBoolean(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool result = false;

  const base::Value* value = GetPreferenceValue(path);
  if (!value) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = value->GetAsBoolean(&result);
  DCHECK(rv);
  return result;
}

int PrefService::GetInteger(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int result = 0;

  const base::Value* value = GetPreferenceValue(path);
  if (!value) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = value->GetAsInteger(&result);
  DCHECK(rv);
  return result;
}

double PrefService::GetDouble(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  double result = 0.0;

  const base::Value* value = GetPreferenceValue(path);
  if (!value) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = value->GetAsDouble(&result);
  DCHECK(rv);
  return result;
}

std::string PrefService::GetString(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string result;

  const base::Value* value = GetPreferenceValue(path);
  if (!value) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = value->GetAsString(&result);
  DCHECK(rv);
  return result;
}

base::FilePath PrefService::GetFilePath(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FilePath result;

  const base::Value* value = GetPreferenceValue(path);
  if (!value) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return base::FilePath(result);
  }
  bool rv = base::GetValueAsFilePath(*value, &result);
  DCHECK(rv);
  return result;
}

bool PrefService::HasPrefPath(const std::string& path) const {
  const Preference* pref = FindPreference(path);
  return pref && !pref->IsDefaultValue();
}

void PrefService::IteratePreferenceValues(
    base::RepeatingCallback<void(const std::string& key,
                                 const base::Value& value)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& it : *pref_registry_)
    callback.Run(it.first, *GetPreferenceValue(it.first));
}

std::unique_ptr<base::DictionaryValue> PrefService::GetPreferenceValues(
    IncludeDefaults include_defaults) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<base::DictionaryValue> out(new base::DictionaryValue);
  for (const auto& it : *pref_registry_) {
    if (include_defaults == INCLUDE_DEFAULTS) {
      out->Set(it.first, GetPreferenceValue(it.first)->CreateDeepCopy());
    } else {
      const Preference* pref = FindPreference(it.first);
      if (pref->IsDefaultValue())
        continue;
      out->Set(it.first, pref->GetValue()->CreateDeepCopy());
    }
  }
  return out;
}

const PrefService::Preference* PrefService::FindPreference(
    const std::string& pref_name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PreferenceMap::iterator it = prefs_map_.find(pref_name);
  if (it != prefs_map_.end())
    return &(it->second);
  const base::Value* default_value = nullptr;
  if (!pref_registry_->defaults()->GetValue(pref_name, &default_value))
    return nullptr;
  it = prefs_map_
           .insert(std::make_pair(
               pref_name, Preference(this, pref_name, default_value->type())))
           .first;
  return &(it->second);
}

bool PrefService::ReadOnly() const {
  return user_pref_store_->ReadOnly();
}

PrefService::PrefInitializationStatus PrefService::GetInitializationStatus()
    const {
  if (!user_pref_store_->IsInitializationComplete())
    return INITIALIZATION_STATUS_WAITING;

  switch (user_pref_store_->GetReadError()) {
    case PersistentPrefStore::PREF_READ_ERROR_NONE:
      return INITIALIZATION_STATUS_SUCCESS;
    case PersistentPrefStore::PREF_READ_ERROR_NO_FILE:
      return INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE;
    default:
      return INITIALIZATION_STATUS_ERROR;
  }
}

PrefService::PrefInitializationStatus
PrefService::GetAllPrefStoresInitializationStatus() const {
  if (!pref_value_store_->IsInitializationComplete())
    return INITIALIZATION_STATUS_WAITING;

  return GetInitializationStatus();
}

bool PrefService::IsManagedPreference(const std::string& pref_name) const {
  const Preference* pref = FindPreference(pref_name);
  return pref && pref->IsManaged();
}

bool PrefService::IsPreferenceManagedByCustodian(
    const std::string& pref_name) const {
  const Preference* pref = FindPreference(pref_name);
  return pref && pref->IsManagedByCustodian();
}

bool PrefService::IsUserModifiablePreference(
    const std::string& pref_name) const {
  const Preference* pref = FindPreference(pref_name);
  return pref && pref->IsUserModifiable();
}

const base::Value* PrefService::Get(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* value = GetPreferenceValue(path);
  if (!value) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return nullptr;
  }
  return value;
}

const base::DictionaryValue* PrefService::GetDictionary(
    const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* value = GetPreferenceValue(path);
  if (!value) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return nullptr;
  }
  if (value->type() != base::Value::Type::DICTIONARY) {
    NOTREACHED();
    return nullptr;
  }
  return static_cast<const base::DictionaryValue*>(value);
}

const base::Value* PrefService::GetUserPrefValue(
    const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return nullptr;
  }

  // Look for an existing preference in the user store. If it doesn't
  // exist, return NULL.
  base::Value* value = nullptr;
  if (!user_pref_store_->GetMutableValue(path, &value))
    return nullptr;

  if (value->type() != pref->GetType()) {
    NOTREACHED() << "Pref value type doesn't match registered type.";
    return nullptr;
  }

  return value;
}

void PrefService::SetDefaultPrefValue(const std::string& path,
                                      base::Value value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_registry_->SetDefaultPrefValue(path, std::move(value));
}

const base::Value* PrefService::GetDefaultPrefValue(
    const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Lookup the preference in the default store.
  const base::Value* value = nullptr;
  bool has_value = pref_registry_->defaults()->GetValue(path, &value);
  DCHECK(has_value) << "Default value missing for pref: " << path;
  return value;
}

const base::ListValue* PrefService::GetList(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* value = GetPreferenceValue(path);
  if (!value) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return nullptr;
  }
  if (value->type() != base::Value::Type::LIST) {
    NOTREACHED();
    return nullptr;
  }
  return static_cast<const base::ListValue*>(value);
}

void PrefService::AddPrefObserver(const std::string& path, PrefObserver* obs) {
  pref_notifier_->AddPrefObserver(path, obs);
}

void PrefService::RemovePrefObserver(const std::string& path,
                                     PrefObserver* obs) {
  pref_notifier_->RemovePrefObserver(path, obs);
}

void PrefService::AddPrefInitObserver(base::OnceCallback<void(bool)> obs) {
  pref_notifier_->AddInitObserver(std::move(obs));
}

PrefRegistry* PrefService::DeprecatedGetPrefRegistry() {
  return pref_registry_.get();
}

void PrefService::ClearPref(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to clear an unregistered pref: " << path;
    return;
  }
  user_pref_store_->RemoveValue(path, GetWriteFlags(pref));
}

void PrefService::ClearMutableValues() {
  user_pref_store_->ClearMutableValues();
}

void PrefService::OnStoreDeletionFromDisk() {
  user_pref_store_->OnStoreDeletionFromDisk();
}

void PrefService::AddPrefObserverAllPrefs(PrefObserver* obs) {
  pref_notifier_->AddPrefObserverAllPrefs(obs);
}

void PrefService::RemovePrefObserverAllPrefs(PrefObserver* obs) {
  pref_notifier_->RemovePrefObserverAllPrefs(obs);
}

void PrefService::Set(const std::string& path, const base::Value& value) {
  SetUserPrefValue(path, value.CreateDeepCopy());
}

void PrefService::SetBoolean(const std::string& path, bool value) {
  SetUserPrefValue(path, std::make_unique<base::Value>(value));
}

void PrefService::SetInteger(const std::string& path, int value) {
  SetUserPrefValue(path, std::make_unique<base::Value>(value));
}

void PrefService::SetDouble(const std::string& path, double value) {
  SetUserPrefValue(path, std::make_unique<base::Value>(value));
}

void PrefService::SetString(const std::string& path, const std::string& value) {
  SetUserPrefValue(path, std::make_unique<base::Value>(value));
}

void PrefService::SetFilePath(const std::string& path,
                              const base::FilePath& value) {
  SetUserPrefValue(path, base::CreateFilePathValue(value));
}

void PrefService::SetInt64(const std::string& path, int64_t value) {
  SetUserPrefValue(path,
                   std::make_unique<base::Value>(base::Int64ToString(value)));
}

int64_t PrefService::GetInt64(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* value = GetPreferenceValue(path);
  if (!value) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return 0;
  }
  std::string result("0");
  bool rv = value->GetAsString(&result);
  DCHECK(rv);

  int64_t val;
  base::StringToInt64(result, &val);
  return val;
}

void PrefService::SetUint64(const std::string& path, uint64_t value) {
  SetUserPrefValue(path,
                   std::make_unique<base::Value>(base::NumberToString(value)));
}

uint64_t PrefService::GetUint64(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* value = GetPreferenceValue(path);
  if (!value) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return 0;
  }
  std::string result("0");
  bool rv = value->GetAsString(&result);
  DCHECK(rv);

  uint64_t val;
  base::StringToUint64(result, &val);
  return val;
}

void PrefService::SetTime(const std::string& path, base::Time value) {
  SetInt64(path, value.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

base::Time PrefService::GetTime(const std::string& path) const {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(GetInt64(path)));
}

void PrefService::SetTimeDelta(const std::string& path, base::TimeDelta value) {
  SetInt64(path, value.InMicroseconds());
}

base::TimeDelta PrefService::GetTimeDelta(const std::string& path) const {
  return base::TimeDelta::FromMicroseconds(GetInt64(path));
}

base::Value* PrefService::GetMutableUserPref(const std::string& path,
                                             base::Value::Type type) {
  CHECK(type == base::Value::Type::DICTIONARY ||
        type == base::Value::Type::LIST);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return nullptr;
  }
  if (pref->GetType() != type) {
    NOTREACHED() << "Wrong type for GetMutableValue: " << path;
    return nullptr;
  }

  // Look for an existing preference in the user store. If it doesn't
  // exist or isn't the correct type, create a new user preference.
  base::Value* value = nullptr;
  if (!user_pref_store_->GetMutableValue(path, &value) ||
      value->type() != type) {
    if (type == base::Value::Type::DICTIONARY) {
      value = new base::DictionaryValue;
    } else if (type == base::Value::Type::LIST) {
      value = new base::ListValue;
    } else {
      NOTREACHED();
    }
    user_pref_store_->SetValueSilently(path, base::WrapUnique(value),
                                       GetWriteFlags(pref));
  }
  return value;
}

void PrefService::ReportUserPrefChanged(const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_pref_store_->ReportValueChanged(key, GetWriteFlags(FindPreference(key)));
}

void PrefService::ReportUserPrefChanged(
    const std::string& key,
    std::set<std::vector<std::string>> path_components) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_pref_store_->ReportSubValuesChanged(key, std::move(path_components),
                                           GetWriteFlags(FindPreference(key)));
}

void PrefService::SetUserPrefValue(const std::string& path,
                                   std::unique_ptr<base::Value> new_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->GetType() != new_value->type()) {
    NOTREACHED() << "Trying to set pref " << path << " of type "
                 << pref->GetType() << " to value of type "
                 << new_value->type();
    return;
  }

  user_pref_store_->SetValue(path, std::move(new_value), GetWriteFlags(pref));
}

void PrefService::UpdateCommandLinePrefStore(PrefStore* command_line_store) {
  pref_value_store_->UpdateCommandLinePrefStore(command_line_store);
}

///////////////////////////////////////////////////////////////////////////////
// PrefService::Preference

PrefService::Preference::Preference(const PrefService* service,
                                    std::string name,
                                    base::Value::Type type)
    : name_(std::move(name)),
      type_(type),
      // Cache the registration flags at creation time to avoid multiple map
      // lookups later.
      registration_flags_(service->pref_registry_->GetRegistrationFlags(name_)),
      pref_service_(service) {}

const base::Value* PrefService::Preference::GetValue() const {
  const base::Value* result = pref_service_->GetPreferenceValue(name_);
  DCHECK(result) << "Must register pref before getting its value";
  return result;
}

const base::Value* PrefService::Preference::GetRecommendedValue() const {
  DCHECK(pref_service_->FindPreference(name_))
      << "Must register pref before getting its value";

  const base::Value* found_value = nullptr;
  if (pref_value_store()->GetRecommendedValue(name_, type_, &found_value)) {
    DCHECK(found_value->type() == type_);
    return found_value;
  }

  // The pref has no recommended value.
  return nullptr;
}

bool PrefService::Preference::IsManaged() const {
  return pref_value_store()->PrefValueInManagedStore(name_);
}

bool PrefService::Preference::IsManagedByCustodian() const {
  return pref_value_store()->PrefValueInSupervisedStore(name_);
}

bool PrefService::Preference::IsRecommended() const {
  return pref_value_store()->PrefValueFromRecommendedStore(name_);
}

bool PrefService::Preference::HasExtensionSetting() const {
  return pref_value_store()->PrefValueInExtensionStore(name_);
}

bool PrefService::Preference::HasUserSetting() const {
  return pref_value_store()->PrefValueInUserStore(name_);
}

bool PrefService::Preference::IsExtensionControlled() const {
  return pref_value_store()->PrefValueFromExtensionStore(name_);
}

bool PrefService::Preference::IsUserControlled() const {
  return pref_value_store()->PrefValueFromUserStore(name_);
}

bool PrefService::Preference::IsDefaultValue() const {
  return pref_value_store()->PrefValueFromDefaultStore(name_);
}

bool PrefService::Preference::IsUserModifiable() const {
  return pref_value_store()->PrefValueUserModifiable(name_);
}

bool PrefService::Preference::IsExtensionModifiable() const {
  return pref_value_store()->PrefValueExtensionModifiable(name_);
}

const base::Value* PrefService::GetPreferenceValue(
    const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(battre): This is a check for crbug.com/435208. After analyzing some
  // crash dumps it looks like the PrefService is accessed even though it has
  // been cleared already.
  CHECK(pref_registry_);
  CHECK(pref_registry_->defaults());
  CHECK(pref_value_store_);

  const base::Value* default_value = nullptr;
  if (pref_registry_->defaults()->GetValue(path, &default_value)) {
    const base::Value* found_value = nullptr;
    base::Value::Type default_type = default_value->type();
    if (pref_value_store_->GetValue(path, default_type, &found_value)) {
      DCHECK(found_value->type() == default_type);
      return found_value;
    } else {
      // Every registered preference has at least a default value.
      NOTREACHED() << "no valid value found for registered pref " << path;
    }
  }

  return nullptr;
}
