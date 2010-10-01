// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service.h"

#include <algorithm>
#include <string>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// A helper function for RegisterLocalized*Pref that creates a Value* based on
// the string value in the locale dll.  Because we control the values in a
// locale dll, this should always return a Value of the appropriate type.
Value* CreateLocaleDefaultValue(Value::ValueType type, int message_id) {
  std::string resource_string = l10n_util::GetStringUTF8(message_id);
  DCHECK(!resource_string.empty());
  switch (type) {
    case Value::TYPE_BOOLEAN: {
      if ("true" == resource_string)
        return Value::CreateBooleanValue(true);
      if ("false" == resource_string)
        return Value::CreateBooleanValue(false);
      break;
    }

    case Value::TYPE_INTEGER: {
      int val;
      base::StringToInt(resource_string, &val);
      return Value::CreateIntegerValue(val);
    }

    case Value::TYPE_REAL: {
      double val;
      base::StringToDouble(resource_string, &val);
      return Value::CreateRealValue(val);
    }

    case Value::TYPE_STRING: {
      return Value::CreateStringValue(resource_string);
    }

    default: {
      NOTREACHED() <<
          "list and dictionary types cannot have default locale values";
    }
  }
  NOTREACHED();
  return Value::CreateNullValue();
}

// Forwards a notification after a PostMessage so that we can wait for the
// MessageLoop to run.
void NotifyReadError(PrefService* pref, int message_id) {
  Source<PrefService> source(pref);
  NotificationService::current()->Notify(NotificationType::PROFILE_ERROR,
                                         source, Details<int>(&message_id));
}

}  // namespace

// static
PrefService* PrefService::CreatePrefService(const FilePath& pref_filename,
                                            Profile* profile) {
#if defined(OS_LINUX)
  // We'd like to see what fraction of our users have the preferences
  // stored on a network file system, as we've had no end of troubles
  // with NFS/AFS.
  // TODO(evanm): remove this once we've collected state.
  file_util::FileSystemType fstype;
  if (file_util::GetFileSystemType(pref_filename.DirName(), &fstype)) {
    UMA_HISTOGRAM_ENUMERATION("PrefService.FileSystemType",
                              static_cast<int>(fstype),
                              file_util::FILE_SYSTEM_TYPE_COUNT);
  }
#endif

  return new PrefService(
      PrefValueStore::CreatePrefValueStore(pref_filename, profile, false));
}

// static
PrefService* PrefService::CreateUserPrefService(const FilePath& pref_filename) {
  return new PrefService(
      PrefValueStore::CreatePrefValueStore(pref_filename, NULL, true));
}

PrefService::PrefService(PrefValueStore* pref_value_store)
    : pref_value_store_(pref_value_store) {
  pref_notifier_.reset(new PrefNotifier(this, pref_value_store));
  InitFromStorage();
}

PrefService::~PrefService() {
  DCHECK(CalledOnValidThread());
  STLDeleteContainerPointers(prefs_.begin(), prefs_.end());
  prefs_.clear();
}

void PrefService::InitFromStorage() {
  PrefStore::PrefReadError error = LoadPersistentPrefs();
  if (error == PrefStore::PREF_READ_ERROR_NONE)
    return;

  // Failing to load prefs on startup is a bad thing(TM). See bug 38352 for
  // an example problem that this can cause.
  // Do some diagnosis and try to avoid losing data.
  int message_id = 0;
  if (error <= PrefStore::PREF_READ_ERROR_JSON_TYPE) {
    message_id = IDS_PREFERENCES_CORRUPT_ERROR;
  } else if (error != PrefStore::PREF_READ_ERROR_NO_FILE) {
    message_id = IDS_PREFERENCES_UNREADABLE_ERROR;
  }

  if (message_id) {
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(&NotifyReadError, this, message_id));
  }
  UMA_HISTOGRAM_ENUMERATION("PrefService.ReadError", error, 20);
}

bool PrefService::ReloadPersistentPrefs() {
  return (LoadPersistentPrefs() == PrefStore::PREF_READ_ERROR_NONE);
}

PrefStore::PrefReadError PrefService::LoadPersistentPrefs() {
  DCHECK(CalledOnValidThread());

  PrefStore::PrefReadError pref_error = pref_value_store_->ReadPrefs();

  for (PreferenceSet::iterator it = prefs_.begin();
       it != prefs_.end(); ++it) {
      (*it)->pref_service_ = this;
  }

  return pref_error;
}

bool PrefService::SavePersistentPrefs() {
  DCHECK(CalledOnValidThread());

  return pref_value_store_->WritePrefs();
}

void PrefService::ScheduleSavePersistentPrefs() {
  DCHECK(CalledOnValidThread());

  pref_value_store_->ScheduleWritePrefs();
}

void PrefService::RegisterBooleanPref(const char* path,
                                      bool default_value) {
  RegisterPreference(path, Value::CreateBooleanValue(default_value));
}

void PrefService::RegisterIntegerPref(const char* path, int default_value) {
  RegisterPreference(path, Value::CreateIntegerValue(default_value));
}

void PrefService::RegisterRealPref(const char* path, double default_value) {
  RegisterPreference(path, Value::CreateRealValue(default_value));
}

void PrefService::RegisterStringPref(const char* path,
                                     const std::string& default_value) {
  RegisterPreference(path, Value::CreateStringValue(default_value));
}

void PrefService::RegisterFilePathPref(const char* path,
                                       const FilePath& default_value) {
  RegisterPreference(path, Value::CreateStringValue(default_value.value()));
}

void PrefService::RegisterListPref(const char* path) {
  RegisterPreference(path, new ListValue());
}

void PrefService::RegisterDictionaryPref(const char* path) {
  RegisterPreference(path, new DictionaryValue());
}

void PrefService::RegisterLocalizedBooleanPref(const char* path,
                                               int locale_default_message_id) {
  RegisterPreference(
      path,
      CreateLocaleDefaultValue(Value::TYPE_BOOLEAN, locale_default_message_id));
}

void PrefService::RegisterLocalizedIntegerPref(const char* path,
                                               int locale_default_message_id) {
  RegisterPreference(
      path,
      CreateLocaleDefaultValue(Value::TYPE_INTEGER, locale_default_message_id));
}

void PrefService::RegisterLocalizedRealPref(const char* path,
                                            int locale_default_message_id) {
  RegisterPreference(
      path,
      CreateLocaleDefaultValue(Value::TYPE_REAL, locale_default_message_id));
}

void PrefService::RegisterLocalizedStringPref(const char* path,
                                              int locale_default_message_id) {
  RegisterPreference(
      path,
      CreateLocaleDefaultValue(Value::TYPE_STRING, locale_default_message_id));
}

bool PrefService::GetBoolean(const char* path) const {
  DCHECK(CalledOnValidThread());

  bool result = false;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsBoolean(&result);
  DCHECK(rv);
  return result;
}

int PrefService::GetInteger(const char* path) const {
  DCHECK(CalledOnValidThread());

  int result = 0;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsInteger(&result);
  DCHECK(rv);
  return result;
}

double PrefService::GetReal(const char* path) const {
  DCHECK(CalledOnValidThread());

  double result = 0.0;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsReal(&result);
  DCHECK(rv);
  return result;
}

std::string PrefService::GetString(const char* path) const {
  DCHECK(CalledOnValidThread());

  std::string result;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsString(&result);
  DCHECK(rv);
  return result;
}

FilePath PrefService::GetFilePath(const char* path) const {
  DCHECK(CalledOnValidThread());

  FilePath::StringType result;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return FilePath(result);
  }
  bool rv = pref->GetValue()->GetAsString(&result);
  DCHECK(rv);
#if defined(OS_POSIX)
  // We store filepaths as UTF8, so convert it back to the system type.
  result = base::SysWideToNativeMB(UTF8ToWide(result));
#endif
  return FilePath(result);
}

bool PrefService::HasPrefPath(const char* path) const {
  return pref_value_store_->HasPrefPath(path);
}

const PrefService::Preference* PrefService::FindPreference(
    const char* pref_name) const {
  DCHECK(CalledOnValidThread());
  Preference p(this, pref_name);
  PreferenceSet::const_iterator it = prefs_.find(&p);
  return it == prefs_.end() ? NULL : *it;
}

bool PrefService::IsManagedPreference(const char* pref_name) const {
  const Preference* pref = FindPreference(pref_name);
  if (pref && pref->IsManaged()) {
    return true;
  }
  return false;
}

const DictionaryValue* PrefService::GetDictionary(const char* path) const {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return NULL;
  }
  const Value* value = pref->GetValue();
  if (value->GetType() == Value::TYPE_NULL)
    return NULL;
  return static_cast<const DictionaryValue*>(value);
}

const ListValue* PrefService::GetList(const char* path) const {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return NULL;
  }
  const Value* value = pref->GetValue();
  if (value->GetType() == Value::TYPE_NULL)
    return NULL;
  return static_cast<const ListValue*>(value);
}

void PrefService::AddPrefObserver(const char* path,
                                  NotificationObserver* obs) {
  pref_notifier_->AddPrefObserver(path, obs);
}

void PrefService::RemovePrefObserver(const char* path,
                                     NotificationObserver* obs) {
  pref_notifier_->RemovePrefObserver(path, obs);
}

void PrefService::RegisterPreference(const char* path, Value* default_value) {
  DCHECK(CalledOnValidThread());

  // The main code path takes ownership, but most don't. We'll be safe.
  scoped_ptr<Value> scoped_value(default_value);

  if (FindPreference(path)) {
    NOTREACHED() << "Tried to register duplicate pref " << path;
    return;
  }

  Value::ValueType orig_type = default_value->GetType();
  DCHECK(orig_type != Value::TYPE_NULL && orig_type != Value::TYPE_BINARY) <<
         "invalid preference type: " << orig_type;

  // We set the default value of dictionaries and lists to be null so it's
  // easier for callers to check for empty dict/list prefs. The PrefValueStore
  // accepts ownership of the value (null or default_value).
  if (Value::TYPE_LIST == orig_type || Value::TYPE_DICTIONARY == orig_type) {
    pref_value_store_->SetDefaultPrefValue(path, Value::CreateNullValue());
  } else {
    // Hand off ownership.
    pref_value_store_->SetDefaultPrefValue(path, scoped_value.release());
  }

  pref_value_store_->RegisterPreferenceType(path, orig_type);
  prefs_.insert(new Preference(this, path));
}

void PrefService::ClearPref(const char* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to clear an unregistered pref: " << path;
    return;
  }
  if (pref_value_store_->RemoveUserPrefValue(path))
    pref_notifier_->OnUserPreferenceSet(path);
}

void PrefService::Set(const char* path, const Value& value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }

  // Allow dictionary and list types to be set to null, which removes their
  // user values.
  bool value_changed = false;
  if (value.GetType() == Value::TYPE_NULL &&
      (pref->GetType() == Value::TYPE_DICTIONARY ||
       pref->GetType() == Value::TYPE_LIST)) {
    value_changed = pref_value_store_->RemoveUserPrefValue(path);
  } else if (pref->GetType() != value.GetType()) {
    NOTREACHED() << "Trying to set pref " << path
                 << " of type " << pref->GetType()
                 << " to value of type " << value.GetType();
  } else {
    value_changed = pref_value_store_->SetUserPrefValue(path, value.DeepCopy());
  }

  if (value_changed)
    pref_notifier_->OnUserPreferenceSet(path);
}

void PrefService::SetBoolean(const char* path, bool value) {
  SetUserPrefValue(path, Value::CreateBooleanValue(value));
}

void PrefService::SetInteger(const char* path, int value) {
  SetUserPrefValue(path, Value::CreateIntegerValue(value));
}

void PrefService::SetReal(const char* path, double value) {
  SetUserPrefValue(path, Value::CreateRealValue(value));
}

void PrefService::SetString(const char* path, const std::string& value) {
  SetUserPrefValue(path, Value::CreateStringValue(value));
}

void PrefService::SetFilePath(const char* path, const FilePath& value) {
#if defined(OS_POSIX)
  // Value::SetString only knows about UTF8 strings, so convert the path from
  // the system native value to UTF8.
  std::string path_utf8 = WideToUTF8(base::SysNativeMBToWide(value.value()));
  Value* new_value = Value::CreateStringValue(path_utf8);
#else
  Value* new_value = Value::CreateStringValue(value.value());
#endif

  SetUserPrefValue(path, new_value);
}

void PrefService::SetInt64(const char* path, int64 value) {
  SetUserPrefValue(path, Value::CreateStringValue(base::Int64ToString(value)));
}

int64 PrefService::GetInt64(const char* path) const {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return 0;
  }
  std::string result("0");
  bool rv = pref->GetValue()->GetAsString(&result);
  DCHECK(rv);

  int64 val;
  base::StringToInt64(result, &val);
  return val;
}

void PrefService::RegisterInt64Pref(const char* path, int64 default_value) {
  RegisterPreference(
      path, Value::CreateStringValue(base::Int64ToString(default_value)));
}

DictionaryValue* PrefService::GetMutableDictionary(const char* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return NULL;
  }
  if (pref->GetType() != Value::TYPE_DICTIONARY) {
    NOTREACHED() << "Wrong type for GetMutableDictionary: " << path;
    return NULL;
  }

  DictionaryValue* dict = NULL;
  Value* tmp_value = NULL;
  // Look for an existing preference in the user store. If it doesn't
  // exist or isn't the correct type, create a new user preference.
  if (!pref_value_store_->GetUserValue(path, &tmp_value) ||
      !tmp_value->IsType(Value::TYPE_DICTIONARY)) {
    dict = new DictionaryValue;
    pref_value_store_->SetUserPrefValue(path, dict);
  } else {
    dict = static_cast<DictionaryValue*>(tmp_value);
  }
  return dict;
}

ListValue* PrefService::GetMutableList(const char* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return NULL;
  }
  if (pref->GetType() != Value::TYPE_LIST) {
    NOTREACHED() << "Wrong type for GetMutableList: " << path;
    return NULL;
  }

  ListValue* list = NULL;
  Value* tmp_value = NULL;
  // Look for an existing preference in the user store. If it doesn't
  // exist or isn't the correct type, create a new user preference.
  if (!pref_value_store_->GetUserValue(path, &tmp_value) ||
      !tmp_value->IsType(Value::TYPE_LIST)) {
    list = new ListValue;
    pref_value_store_->SetUserPrefValue(path, list);
  } else {
    list = static_cast<ListValue*>(tmp_value);
  }
  return list;
}

Value* PrefService::GetPrefCopy(const char* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  DCHECK(pref);
  return pref->GetValue()->DeepCopy();
}

void PrefService::SetUserPrefValue(const char* path, Value* new_value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->IsManaged()) {
    NOTREACHED() << "Preference is managed: " << path;
    return;
  }
  if (pref->GetType() != new_value->GetType()) {
    NOTREACHED() << "Trying to set pref " << path
                 << " of type " << pref->GetType()
                 << " to value of type " << new_value->GetType();
    return;
  }

  if (pref_value_store_->SetUserPrefValue(path, new_value))
    pref_notifier_->OnUserPreferenceSet(path);
}

///////////////////////////////////////////////////////////////////////////////
// PrefService::Preference

PrefService::Preference::Preference(const PrefService* service,
                                    const char* name)
      : name_(name),
        pref_service_(service) {
  DCHECK(name);
  DCHECK(service);
}

Value::ValueType PrefService::Preference::GetType() const {
  return pref_service_->pref_value_store_->GetRegisteredType(name_);
}

const Value* PrefService::Preference::GetValue() const {
  DCHECK(pref_service_->FindPreference(name_.c_str())) <<
      "Must register pref before getting its value";

  Value* found_value = NULL;
  if (pref_service_->pref_value_store_->GetValue(name_, &found_value))
    return found_value;

  // Every registered preference has at least a default value.
  NOTREACHED() << "no valid value found for registered pref " << name_;
  return NULL;
}

bool PrefService::Preference::IsManaged() const {
  return pref_service_->pref_value_store_->
      PrefValueInManagedStore(name_.c_str());
}

bool PrefService::Preference::HasExtensionSetting() const {
  return pref_service_->pref_value_store_->
      PrefValueInExtensionStore(name_.c_str());
}

bool PrefService::Preference::HasUserSetting() const {
  return pref_service_->pref_value_store_->
      PrefValueInUserStore(name_.c_str());
}

bool PrefService::Preference::IsExtensionControlled() const {
  return pref_service_->pref_value_store_->
      PrefValueFromExtensionStore(name_.c_str());
}

bool PrefService::Preference::IsUserControlled() const {
  return pref_service_->pref_value_store_->
      PrefValueFromUserStore(name_.c_str());
}

bool PrefService::Preference::IsDefaultValue() const {
  return pref_service_->pref_value_store_->
      PrefValueFromDefaultStore(name_.c_str());
}

bool PrefService::Preference::IsUserModifiable() const {
  return pref_service_->pref_value_store_->
      PrefValueUserModifiable(name_.c_str());
}
