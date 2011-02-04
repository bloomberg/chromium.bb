// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/browser/prefs/overlay_persistent_pref_store.h"
#include "chrome/browser/prefs/pref_notifier_impl.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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

    case Value::TYPE_DOUBLE: {
      double val;
      base::StringToDouble(resource_string, &val);
      return Value::CreateDoubleValue(val);
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
                                            PrefStore* extension_prefs,
                                            Profile* profile) {
  using policy::ConfigurationPolicyPrefStore;

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

  ConfigurationPolicyPrefStore* managed_platform =
      ConfigurationPolicyPrefStore::CreateManagedPlatformPolicyPrefStore();
  ConfigurationPolicyPrefStore* managed_cloud =
      ConfigurationPolicyPrefStore::CreateManagedCloudPolicyPrefStore(profile);
  CommandLinePrefStore* command_line =
      new CommandLinePrefStore(CommandLine::ForCurrentProcess());
  JsonPrefStore* user = new JsonPrefStore(
      pref_filename,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  ConfigurationPolicyPrefStore* recommended_platform =
      ConfigurationPolicyPrefStore::CreateRecommendedPlatformPolicyPrefStore();
  ConfigurationPolicyPrefStore* recommended_cloud =
      ConfigurationPolicyPrefStore::CreateRecommendedCloudPolicyPrefStore(
          profile);
  DefaultPrefStore* default_pref_store = new DefaultPrefStore();

  return new PrefService(managed_platform, managed_cloud, extension_prefs,
                         command_line, user, recommended_platform,
                         recommended_cloud, default_pref_store);
}

PrefService* PrefService::CreateIncognitoPrefService(
    PrefStore* incognito_extension_prefs) {
  return new PrefService(*this, incognito_extension_prefs);
}

PrefService::PrefService(PrefStore* managed_platform_prefs,
                         PrefStore* managed_cloud_prefs,
                         PrefStore* extension_prefs,
                         PrefStore* command_line_prefs,
                         PersistentPrefStore* user_prefs,
                         PrefStore* recommended_platform_prefs,
                         PrefStore* recommended_cloud_prefs,
                         DefaultPrefStore* default_store)
    : user_pref_store_(user_prefs),
      default_store_(default_store) {
  pref_notifier_.reset(new PrefNotifierImpl(this));
  pref_value_store_.reset(
      new PrefValueStore(managed_platform_prefs,
                         managed_cloud_prefs,
                         extension_prefs,
                         command_line_prefs,
                         user_pref_store_,
                         recommended_platform_prefs,
                         recommended_cloud_prefs,
                         default_store,
                         pref_notifier_.get()));
  InitFromStorage();
}

PrefService::PrefService(const PrefService& original,
                         PrefStore* incognito_extension_prefs)
      : user_pref_store_(
            new OverlayPersistentPrefStore(original.user_pref_store_.get())),
        default_store_(original.default_store_.get()){
  pref_notifier_.reset(new PrefNotifierImpl(this));
  pref_value_store_.reset(original.pref_value_store_->CloneAndSpecialize(
      NULL, // managed_platform_prefs
      NULL, // managed_cloud_prefs
      incognito_extension_prefs,
      NULL, // command_line_prefs
      user_pref_store_.get(),
      NULL, // recommended_platform_prefs
      NULL, // recommended_cloud_prefs
      default_store_.get(),
      pref_notifier_.get()));
  InitFromStorage();
}

PrefService::~PrefService() {
  DCHECK(CalledOnValidThread());
  STLDeleteContainerPointers(prefs_.begin(), prefs_.end());
  prefs_.clear();

  // Reset pointers so accesses after destruction reliably crash.
  pref_value_store_.reset();
  user_pref_store_ = NULL;
  default_store_ = NULL;
}

void PrefService::InitFromStorage() {
  const PersistentPrefStore::PrefReadError error =
      user_pref_store_->ReadPrefs();
  if (error == PersistentPrefStore::PREF_READ_ERROR_NONE)
    return;

  // Failing to load prefs on startup is a bad thing(TM). See bug 38352 for
  // an example problem that this can cause.
  // Do some diagnosis and try to avoid losing data.
  int message_id = 0;
  if (error <= PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE) {
    message_id = IDS_PREFERENCES_CORRUPT_ERROR;
  } else if (error != PersistentPrefStore::PREF_READ_ERROR_NO_FILE) {
    message_id = IDS_PREFERENCES_UNREADABLE_ERROR;
  }

  if (message_id) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&NotifyReadError, this, message_id));
  }
  UMA_HISTOGRAM_ENUMERATION("PrefService.ReadError", error, 20);
}

bool PrefService::ReloadPersistentPrefs() {
  return user_pref_store_->ReadPrefs() ==
             PersistentPrefStore::PREF_READ_ERROR_NONE;
}

bool PrefService::SavePersistentPrefs() {
  DCHECK(CalledOnValidThread());

  return user_pref_store_->WritePrefs();
}

void PrefService::ScheduleSavePersistentPrefs() {
  DCHECK(CalledOnValidThread());

  user_pref_store_->ScheduleWritePrefs();
}

void PrefService::RegisterBooleanPref(const char* path,
                                      bool default_value) {
  RegisterPreference(path, Value::CreateBooleanValue(default_value));
}

void PrefService::RegisterIntegerPref(const char* path, int default_value) {
  RegisterPreference(path, Value::CreateIntegerValue(default_value));
}

void PrefService::RegisterDoublePref(const char* path, double default_value) {
  RegisterPreference(path, Value::CreateDoubleValue(default_value));
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

void PrefService::RegisterLocalizedDoublePref(const char* path,
                                              int locale_default_message_id) {
  RegisterPreference(
      path,
      CreateLocaleDefaultValue(Value::TYPE_DOUBLE, locale_default_message_id));
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

double PrefService::GetDouble(const char* path) const {
  DCHECK(CalledOnValidThread());

  double result = 0.0;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsDouble(&result);
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
  const Preference* pref = FindPreference(path);
  return pref && !pref->IsDefaultValue();
}

DictionaryValue* PrefService::GetPreferenceValues() const {
  DCHECK(CalledOnValidThread());
  DictionaryValue* out = new DictionaryValue;
  DefaultPrefStore::const_iterator i = default_store_->begin();
  for (; i != default_store_->end(); ++i) {
    const Value* value = FindPreference(i->first.c_str())->GetValue();
    out->Set(i->first, value->DeepCopy());
  }
  return out;
}

const PrefService::Preference* PrefService::FindPreference(
    const char* pref_name) const {
  DCHECK(CalledOnValidThread());
  Preference p(this, pref_name, Value::TYPE_NULL);
  PreferenceSet::const_iterator it = prefs_.find(&p);
  if (it != prefs_.end())
    return *it;
  const Value::ValueType type = default_store_->GetType(pref_name);
  if (type == Value::TYPE_NULL)
    return NULL;
  Preference* new_pref = new Preference(this, pref_name, type);
  prefs_.insert(new_pref);
  return new_pref;
}

bool PrefService::ReadOnly() const {
  return user_pref_store_->ReadOnly();
}

PrefNotifier* PrefService::pref_notifier() const {
  return pref_notifier_.get();
}

bool PrefService::IsManagedPreference(const char* pref_name) const {
  const Preference* pref = FindPreference(pref_name);
  return pref && pref->IsManaged();
}

const DictionaryValue* PrefService::GetDictionary(const char* path) const {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return NULL;
  }
  const Value* value = pref->GetValue();
  if (value->GetType() != Value::TYPE_DICTIONARY) {
    NOTREACHED();
    return NULL;
  }
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
  if (value->GetType() != Value::TYPE_LIST) {
    NOTREACHED();
    return NULL;
  }
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

  // Hand off ownership.
  default_store_->SetDefaultValue(path, scoped_value.release());
}

void PrefService::ClearPref(const char* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to clear an unregistered pref: " << path;
    return;
  }
  user_pref_store_->RemoveValue(path);
}

void PrefService::Set(const char* path, const Value& value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }

  if (pref->GetType() != value.GetType()) {
    NOTREACHED() << "Trying to set pref " << path
                 << " of type " << pref->GetType()
                 << " to value of type " << value.GetType();
  } else {
    user_pref_store_->SetValue(path, value.DeepCopy());
  }
}

void PrefService::SetBoolean(const char* path, bool value) {
  SetUserPrefValue(path, Value::CreateBooleanValue(value));
}

void PrefService::SetInteger(const char* path, int value) {
  SetUserPrefValue(path, Value::CreateIntegerValue(value));
}

void PrefService::SetDouble(const char* path, double value) {
  SetUserPrefValue(path, Value::CreateDoubleValue(value));
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
  if (user_pref_store_->GetValue(path, &tmp_value)
          != PersistentPrefStore::READ_OK ||
      !tmp_value->IsType(Value::TYPE_DICTIONARY)) {
    dict = new DictionaryValue;
    user_pref_store_->SetValueSilently(path, dict);
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
  if (user_pref_store_->GetValue(path, &tmp_value)
          != PersistentPrefStore::READ_OK ||
      !tmp_value->IsType(Value::TYPE_LIST)) {
    list = new ListValue;
    user_pref_store_->SetValueSilently(path, list);
  } else {
    list = static_cast<ListValue*>(tmp_value);
  }
  return list;
}

void PrefService::SetUserPrefValue(const char* path, Value* new_value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->GetType() != new_value->GetType()) {
    NOTREACHED() << "Trying to set pref " << path
                 << " of type " << pref->GetType()
                 << " to value of type " << new_value->GetType();
    return;
  }

  user_pref_store_->SetValue(path, new_value);
}

///////////////////////////////////////////////////////////////////////////////
// PrefService::Preference

PrefService::Preference::Preference(const PrefService* service,
                                    const char* name,
                                    Value::ValueType type)
      : name_(name),
        type_(type),
        pref_service_(service) {
  DCHECK(name);
  DCHECK(service);
}

Value::ValueType PrefService::Preference::GetType() const {
  return type_;
}

const Value* PrefService::Preference::GetValue() const {
  DCHECK(pref_service_->FindPreference(name_.c_str())) <<
      "Must register pref before getting its value";

  Value* found_value = NULL;
  if (pref_value_store()->GetValue(name_, type_, &found_value)) {
    DCHECK(found_value->IsType(type_));
    return found_value;
  }

  // Every registered preference has at least a default value.
  NOTREACHED() << "no valid value found for registered pref " << name_;
  return NULL;
}

bool PrefService::Preference::IsManaged() const {
  return pref_value_store()->PrefValueInManagedStore(name_.c_str());
}

bool PrefService::Preference::HasExtensionSetting() const {
  return pref_value_store()->PrefValueInExtensionStore(name_.c_str());
}

bool PrefService::Preference::HasUserSetting() const {
  return pref_value_store()->PrefValueInUserStore(name_.c_str());
}

bool PrefService::Preference::IsExtensionControlled() const {
  return pref_value_store()->PrefValueFromExtensionStore(name_.c_str());
}

bool PrefService::Preference::IsUserControlled() const {
  return pref_value_store()->PrefValueFromUserStore(name_.c_str());
}

bool PrefService::Preference::IsDefaultValue() const {
  return pref_value_store()->PrefValueFromDefaultStore(name_.c_str());
}

bool PrefService::Preference::IsUserModifiable() const {
  return pref_value_store()->PrefValueUserModifiable(name_.c_str());
}
