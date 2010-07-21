// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pref_service.h"

#include <algorithm>
#include <string>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_thread.h"

#if defined(OS_WIN)
#include "chrome/browser/configuration_policy_provider_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/configuration_policy_provider_mac.h"
#elif defined(OS_POSIX)
#include "chrome/browser/config_dir_policy_provider.h"
#endif
#include "chrome/browser/dummy_configuration_policy_provider.h"

#include "chrome/browser/command_line_pref_store.h"
#include "chrome/browser/configuration_policy_pref_store.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// A helper function for RegisterLocalized*Pref that creates a Value* based on
// the string value in the locale dll.  Because we control the values in a
// locale dll, this should always return a Value of the appropriate type.
Value* CreateLocaleDefaultValue(Value::ValueType type, int message_id) {
  std::wstring resource_string = l10n_util::GetString(message_id);
  DCHECK(!resource_string.empty());
  switch (type) {
    case Value::TYPE_BOOLEAN: {
      if (L"true" == resource_string)
        return Value::CreateBooleanValue(true);
      if (L"false" == resource_string)
        return Value::CreateBooleanValue(false);
      break;
    }

    case Value::TYPE_INTEGER: {
      return Value::CreateIntegerValue(
          StringToInt(WideToUTF16Hack(resource_string)));
      break;
    }

    case Value::TYPE_REAL: {
      return Value::CreateRealValue(
          StringToDouble(WideToUTF16Hack(resource_string)));
      break;
    }

    case Value::TYPE_STRING: {
      return Value::CreateStringValue(resource_string);
      break;
    }

    default: {
      NOTREACHED() <<
          "list and dictionary types can not have default locale values";
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
PrefService* PrefService::CreatePrefService(const FilePath& pref_filename) {
  PrefStore* managed_prefs = NULL;
  // TODO(pamg): Reinstate extension pref store after Mstone6 branch, when its
  // memory leaks are fixed and any extension API is using it.
  // ExtensionPrefStore* extension_prefs = new ExtensionPrefStore(NULL);
  ExtensionPrefStore* extension_prefs = NULL;
  CommandLinePrefStore* command_line_prefs = new CommandLinePrefStore(
      CommandLine::ForCurrentProcess());
  PrefStore* local_prefs = new JsonPrefStore(
      pref_filename,
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE));
  PrefStore* recommended_prefs = NULL;

  ConfigurationPolicyProvider* managed_prefs_provider = NULL;
  ConfigurationPolicyProvider* recommended_prefs_provider = NULL;

#if defined(OS_WIN)
  managed_prefs_provider = new ConfigurationPolicyProviderWin();
  recommended_prefs_provider = new DummyConfigurationPolicyProvider();
#elif defined(OS_MACOSX)
  managed_prefs_provider = new ConfigurationPolicyProviderMac();
  recommended_prefs_provider = new DummyConfigurationPolicyProvider();
#elif defined(OS_POSIX)
  FilePath config_dir_path;
  if (PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    managed_prefs_provider = new ConfigDirPolicyProvider(
        config_dir_path.Append(FILE_PATH_LITERAL("managed")));
    recommended_prefs_provider = new ConfigDirPolicyProvider(
        config_dir_path.Append(FILE_PATH_LITERAL("recommended")));
  } else {
    managed_prefs_provider = new DummyConfigurationPolicyProvider();
    recommended_prefs_provider = new DummyConfigurationPolicyProvider();
  }
#endif

  // The ConfigurationPolicyPrefStore takes ownership of the passed
  // |provider|.
  managed_prefs = new ConfigurationPolicyPrefStore(
      CommandLine::ForCurrentProcess(),
      managed_prefs_provider);
  recommended_prefs = new ConfigurationPolicyPrefStore(
      CommandLine::ForCurrentProcess(),
      recommended_prefs_provider);

  // The PrefValueStore takes ownership of the PrefStores.
  PrefValueStore* value_store = new PrefValueStore(
      managed_prefs,
      extension_prefs,
      command_line_prefs,
      local_prefs,
      recommended_prefs);

  PrefService* pref_service = new PrefService(value_store);
  // TODO(pamg): Uncomment when ExtensionPrefStore is reinstated.
  // extension_prefs->SetPrefService(pref_service);

  return pref_service;
}

// static
PrefService* PrefService::CreateUserPrefService(
    const FilePath& pref_filename) {
  PrefValueStore* value_store = new PrefValueStore(
      NULL, /* no enforced prefs */
      NULL, /* no extension prefs */
      NULL, /* no command-line prefs */
      new JsonPrefStore(
          pref_filename,
          ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE)),
          /* user prefs */
      NULL /* no advised prefs */);
  return new PrefService(value_store);
}

PrefService::PrefService(PrefValueStore* pref_value_store)
    : pref_value_store_(pref_value_store) {
  InitFromStorage();
}

PrefService::~PrefService() {
  DCHECK(CalledOnValidThread());

  // Verify that there are no pref observers when we shut down.
  for (PrefObserverMap::iterator it = pref_observers_.begin();
       it != pref_observers_.end(); ++it) {
    NotificationObserverList::Iterator obs_iterator(*(it->second));
    if (obs_iterator.GetNext()) {
      LOG(WARNING) << "pref observer found at shutdown " << it->first;
    }
  }

  STLDeleteContainerPointers(prefs_.begin(), prefs_.end());
  prefs_.clear();
  STLDeleteContainerPairSecondPointers(pref_observers_.begin(),
                                       pref_observers_.end());
  pref_observers_.clear();
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
    (*it)->pref_value_store_ = pref_value_store_.get();
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

void PrefService::RegisterBooleanPref(const wchar_t* path,
                                      bool default_value) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      Value::CreateBooleanValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterIntegerPref(const wchar_t* path,
                                      int default_value) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      Value::CreateIntegerValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterRealPref(const wchar_t* path,
                                   double default_value) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      Value::CreateRealValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterStringPref(const wchar_t* path,
                                     const std::string& default_value) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      Value::CreateStringValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterFilePathPref(const wchar_t* path,
                                       const FilePath& default_value) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      Value::CreateStringValue(default_value.value()));
  RegisterPreference(pref);
}

void PrefService::RegisterListPref(const wchar_t* path) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      new ListValue);
  RegisterPreference(pref);
}

void PrefService::RegisterDictionaryPref(const wchar_t* path) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      new DictionaryValue());
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedBooleanPref(const wchar_t* path,
                                               int locale_default_message_id) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_BOOLEAN, locale_default_message_id));
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedIntegerPref(const wchar_t* path,
                                               int locale_default_message_id) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_INTEGER, locale_default_message_id));
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedRealPref(const wchar_t* path,
                                            int locale_default_message_id) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_REAL, locale_default_message_id));
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedStringPref(const wchar_t* path,
                                              int locale_default_message_id) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_STRING, locale_default_message_id));
  RegisterPreference(pref);
}

bool PrefService::GetBoolean(const wchar_t* path) const {
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

int PrefService::GetInteger(const wchar_t* path) const {
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

double PrefService::GetReal(const wchar_t* path) const {
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

std::string PrefService::GetString(const wchar_t* path) const {
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

FilePath PrefService::GetFilePath(const wchar_t* path) const {
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

bool PrefService::HasPrefPath(const wchar_t* path) const {
  return pref_value_store_->HasPrefPath(path);
}

const PrefService::Preference* PrefService::FindPreference(
    const wchar_t* pref_name) const {
  DCHECK(CalledOnValidThread());
  Preference p(NULL, pref_name, NULL);
  PreferenceSet::const_iterator it = prefs_.find(&p);
  return it == prefs_.end() ? NULL : *it;
}

bool PrefService::IsManagedPreference(const wchar_t* pref_name) const {
  const Preference* pref = FindPreference(pref_name);
  if (pref && pref->IsManaged()) {
    return true;
  }
  return false;
}

void PrefService::FireObserversIfChanged(const wchar_t* path,
                                         const Value* old_value) {
  if (PrefIsChanged(path, old_value))
    FireObservers(path);
}

void PrefService::FireObservers(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  // Convert path to a std::wstring because the Details constructor requires a
  // class.
  std::wstring path_str(path);
  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path_str);
  if (observer_iterator == pref_observers_.end())
    return;

  NotificationObserverList::Iterator it(*(observer_iterator->second));
  NotificationObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    observer->Observe(NotificationType::PREF_CHANGED,
                      Source<PrefService>(this),
                      Details<std::wstring>(&path_str));
  }
}

bool PrefService::PrefIsChanged(const wchar_t* path,
                                const Value* old_value) {
  Value* new_value = NULL;
  pref_value_store_->GetValue(path, &new_value);
  // Some unit tests have no values for certain prefs.
  return (!new_value || !old_value->Equals(new_value));
}

const DictionaryValue* PrefService::GetDictionary(const wchar_t* path) const {
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

const ListValue* PrefService::GetList(const wchar_t* path) const {
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

void PrefService::AddPrefObserver(const wchar_t* path,
                                  NotificationObserver* obs) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to add an observer for an unregistered pref: "
        << path;
    return;
  }

  // Get the pref observer list associated with the path.
  NotificationObserverList* observer_list = NULL;
  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end()) {
    observer_list = new NotificationObserverList;
    pref_observers_[path] = observer_list;
  } else {
    observer_list = observer_iterator->second;
  }

  // Verify that this observer doesn't already exist.
  NotificationObserverList::Iterator it(*observer_list);
  NotificationObserver* existing_obs;
  while ((existing_obs = it.GetNext()) != NULL) {
    DCHECK(existing_obs != obs) << path << " observer already registered";
    if (existing_obs == obs)
      return;
  }

  // Ok, safe to add the pref observer.
  observer_list->AddObserver(obs);
}

void PrefService::RemovePrefObserver(const wchar_t* path,
                                     NotificationObserver* obs) {
  DCHECK(CalledOnValidThread());

  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end()) {
    return;
  }

  NotificationObserverList* observer_list = observer_iterator->second;
  observer_list->RemoveObserver(obs);
}

void PrefService::RegisterPreference(Preference* pref) {
  DCHECK(CalledOnValidThread());

  if (FindPreference(pref->name().c_str())) {
    NOTREACHED() << "Tried to register duplicate pref " << pref->name();
    delete pref;
    return;
  }
  prefs_.insert(pref);
}

void PrefService::ClearPref(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to clear an unregistered pref: " << path;
    return;
  }
  Value* value;
  bool has_old_value = pref_value_store_->GetValue(path, &value);
  pref_value_store_->RemoveUserPrefValue(path);

  if (has_old_value)
    FireObservers(path);
}

void PrefService::Set(const wchar_t* path, const Value& value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }

  // Allow dictionary and list types to be set to null.
  if (value.GetType() == Value::TYPE_NULL &&
      (pref->type() == Value::TYPE_DICTIONARY ||
       pref->type() == Value::TYPE_LIST)) {
    scoped_ptr<Value> old_value(GetPrefCopy(path));
    if (!old_value->Equals(&value)) {
      pref_value_store_->RemoveUserPrefValue(path);
      FireObservers(path);
    }
    return;
  }

  if (pref->type() != value.GetType()) {
    NOTREACHED() << "Wrong type for Set: " << path;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  pref_value_store_->SetUserPrefValue(path, value.DeepCopy());

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetBoolean(const wchar_t* path, bool value) {
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
  if (pref->type() != Value::TYPE_BOOLEAN) {
    NOTREACHED() << "Wrong type for SetBoolean: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  Value* new_value = Value::CreateBooleanValue(value);
  pref_value_store_->SetUserPrefValue(path, new_value);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetInteger(const wchar_t* path, int value) {
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
  if (pref->type() != Value::TYPE_INTEGER) {
    NOTREACHED() << "Wrong type for SetInteger: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  Value* new_value = Value::CreateIntegerValue(value);
  pref_value_store_->SetUserPrefValue(path, new_value);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetReal(const wchar_t* path, double value) {
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
  if (pref->type() != Value::TYPE_REAL) {
    NOTREACHED() << "Wrong type for SetReal: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  Value* new_value = Value::CreateRealValue(value);
  pref_value_store_->SetUserPrefValue(path, new_value);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetString(const wchar_t* path, const std::string& value) {
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
  if (pref->type() != Value::TYPE_STRING) {
    NOTREACHED() << "Wrong type for SetString: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  Value* new_value = Value::CreateStringValue(value);
  pref_value_store_->SetUserPrefValue(path, new_value);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetFilePath(const wchar_t* path, const FilePath& value) {
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
  if (pref->type() != Value::TYPE_STRING) {
    NOTREACHED() << "Wrong type for SetFilePath: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
#if defined(OS_POSIX)
  // Value::SetString only knows about UTF8 strings, so convert the path from
  // the system native value to UTF8.
  std::string path_utf8 = WideToUTF8(base::SysNativeMBToWide(value.value()));
  Value* new_value = Value::CreateStringValue(path_utf8);
  pref_value_store_->SetUserPrefValue(path, new_value);
#else
  Value* new_value = Value::CreateStringValue(value.value());
  pref_value_store_->SetUserPrefValue(path, new_value);
#endif

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetInt64(const wchar_t* path, int64 value) {
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
  if (pref->type() != Value::TYPE_STRING) {
    NOTREACHED() << "Wrong type for SetInt64: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  Value* new_value = Value::CreateStringValue(Int64ToWString(value));
  pref_value_store_->SetUserPrefValue(path, new_value);

  FireObserversIfChanged(path, old_value.get());
}

int64 PrefService::GetInt64(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return 0;
  }
  std::wstring result(L"0");
  bool rv = pref->GetValue()->GetAsString(&result);
  DCHECK(rv);
  return StringToInt64(WideToUTF16Hack(result));
}

void PrefService::RegisterInt64Pref(const wchar_t* path, int64 default_value) {
  Preference* pref = new Preference(pref_value_store_.get(), path,
      Value::CreateStringValue(Int64ToWString(default_value)));
  RegisterPreference(pref);
}

DictionaryValue* PrefService::GetMutableDictionary(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return NULL;
  }
  if (pref->type() != Value::TYPE_DICTIONARY) {
    NOTREACHED() << "Wrong type for GetMutableDictionary: " << path;
    return NULL;
  }

  DictionaryValue* dict = NULL;
  Value* tmp_value = NULL;
  if (!pref_value_store_->GetValue(path, &tmp_value) ||
      !tmp_value->IsType(Value::TYPE_DICTIONARY)) {
    dict = new DictionaryValue;
    pref_value_store_->SetUserPrefValue(path, dict);
  } else {
    dict = static_cast<DictionaryValue*>(tmp_value);
  }
  return dict;
}

ListValue* PrefService::GetMutableList(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return NULL;
  }
  if (pref->type() != Value::TYPE_LIST) {
    NOTREACHED() << "Wrong type for GetMutableList: " << path;
    return NULL;
  }

  ListValue* list = NULL;
  Value* tmp_value = NULL;
  if (!pref_value_store_->GetValue(path, &tmp_value)) {
    list = new ListValue;
    pref_value_store_->SetUserPrefValue(path, list);
  } else {
    list = static_cast<ListValue*>(tmp_value);
  }
  return list;
}

Value* PrefService::GetPrefCopy(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  DCHECK(pref);
  return pref->GetValue()->DeepCopy();
}

///////////////////////////////////////////////////////////////////////////////
// PrefService::Preference

PrefService::Preference::Preference(PrefValueStore* pref_value_store,
                                    const wchar_t* name,
                                    Value* default_value)
      : type_(Value::TYPE_NULL),
        name_(name),
        default_value_(default_value),
        pref_value_store_(pref_value_store) {
  DCHECK(name);

  if (default_value) {
    type_ = default_value->GetType();
    DCHECK(type_ != Value::TYPE_NULL && type_ != Value::TYPE_BINARY) <<
        "invalid preference type: " << type_;
  }

  // We set the default value of lists and dictionaries to be null so it's
  // easier for callers to check for empty list/dict prefs.
  if (Value::TYPE_LIST == type_ || Value::TYPE_DICTIONARY == type_)
    default_value_.reset(Value::CreateNullValue());
}

const Value* PrefService::Preference::GetValue() const {
  DCHECK(NULL != pref_value_store_) <<
      "Must register pref before getting its value";

  Value* temp_value = NULL;
  if (pref_value_store_->GetValue(name_, &temp_value) &&
      temp_value->GetType() == type_) {
    return temp_value;
  }

  // Pref not found, just return the app default.
  return default_value_.get();
}

bool PrefService::Preference::IsDefaultValue() const {
  DCHECK(default_value_.get());
  return default_value_->Equals(GetValue());
}

bool PrefService::Preference::IsManaged() const {
  return pref_value_store_->PrefValueInManagedStore(name_.c_str());
}

bool PrefService::Preference::HasExtensionSetting() const {
  return pref_value_store_->PrefValueInExtensionStore(name_.c_str());
}

bool PrefService::Preference::HasUserSetting() const {
  return pref_value_store_->PrefValueInUserStore(name_.c_str());
}

bool PrefService::Preference::IsExtensionControlled() const {
  return pref_value_store_->PrefValueFromExtensionStore(name_.c_str());
}

bool PrefService::Preference::IsUserControlled() const {
  return pref_value_store_->PrefValueFromUserStore(name_.c_str());
}

bool PrefService::Preference::IsUserModifiable() const {
  return pref_value_store_->PrefValueUserModifiable(name_.c_str());
}
