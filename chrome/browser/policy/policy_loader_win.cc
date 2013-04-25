// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_loader_win.h"

#include <rpc.h>      // For struct GUID
#include <shlwapi.h>  // For PathIsUNC()
#include <userenv.h>  // For GPO functions
#include <windows.h>

#include <string>
#include <vector>

// shlwapi.dll is required for PathIsUNC().
#pragma comment(lib, "shlwapi.lib")
// userenv.dll is required for various GPO functions.
#pragma comment(lib, "userenv.lib")

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_load_status.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/preg_parser_win.h"
#include "chrome/common/json_schema/json_schema_constants.h"
#include "policy/policy_constants.h"

namespace schema = json_schema_constants;

using base::win::RegKey;
using base::win::RegistryKeyIterator;
using base::win::RegistryValueIterator;

namespace policy {

namespace {

const char kKeyMandatory[] = "policy";
const char kKeyRecommended[] = "recommended";
const char kKeySchema[] = "schema";
const char kKeyThirdParty[] = "3rdparty";

// The GUID of the registry settings group policy extension.
GUID kRegistrySettingsCSEGUID = REGISTRY_EXTENSION_GUID;

// A helper class encapsulating run-time-linked function calls to Wow64 APIs.
class Wow64Functions {
 public:
  Wow64Functions()
    : kernel32_lib_(base::FilePath(L"kernel32")),
      is_wow_64_process_(NULL),
      wow_64_disable_wow_64_fs_redirection_(NULL),
      wow_64_revert_wow_64_fs_redirection_(NULL) {
    if (kernel32_lib_.is_valid()) {
      is_wow_64_process_ = static_cast<IsWow64Process>(
          kernel32_lib_.GetFunctionPointer("IsWow64Process"));
      wow_64_disable_wow_64_fs_redirection_ =
          static_cast<Wow64DisableWow64FSRedirection>(
              kernel32_lib_.GetFunctionPointer(
                  "Wow64DisableWow64FsRedirection"));
      wow_64_revert_wow_64_fs_redirection_ =
          static_cast<Wow64RevertWow64FSRedirection>(
              kernel32_lib_.GetFunctionPointer(
                  "Wow64RevertWow64FsRedirection"));
    }
  }

  bool is_valid() {
    return is_wow_64_process_ &&
        wow_64_disable_wow_64_fs_redirection_ &&
        wow_64_revert_wow_64_fs_redirection_;
 }

  bool IsWow64() {
    BOOL result = 0;
    if (!is_wow_64_process_(GetCurrentProcess(), &result))
      PLOG(WARNING) << "IsWow64ProcFailed";
    return !!result;
  }

  bool DisableFsRedirection(PVOID* previous_state) {
    return !!wow_64_disable_wow_64_fs_redirection_(previous_state);
  }

  bool RevertFsRedirection(PVOID previous_state) {
    return !!wow_64_revert_wow_64_fs_redirection_(previous_state);
  }

 private:
  typedef BOOL (WINAPI* IsWow64Process)(HANDLE, PBOOL);
  typedef BOOL (WINAPI* Wow64DisableWow64FSRedirection)(PVOID*);
  typedef BOOL (WINAPI* Wow64RevertWow64FSRedirection)(PVOID);

  base::ScopedNativeLibrary kernel32_lib_;

  IsWow64Process is_wow_64_process_;
  Wow64DisableWow64FSRedirection wow_64_disable_wow_64_fs_redirection_;
  Wow64RevertWow64FSRedirection wow_64_revert_wow_64_fs_redirection_;

  DISALLOW_COPY_AND_ASSIGN(Wow64Functions);
};

// Global Wow64Function instance used by ScopedDisableWow64Redirection below.
static base::LazyInstance<Wow64Functions> g_wow_64_functions =
    LAZY_INSTANCE_INITIALIZER;

// Scoper that switches off Wow64 File System Redirection during its lifetime.
class ScopedDisableWow64Redirection {
 public:
  ScopedDisableWow64Redirection()
    : active_(false),
      previous_state_(NULL) {
    Wow64Functions* wow64 = g_wow_64_functions.Pointer();
    if (wow64->is_valid() && wow64->IsWow64()) {
      if (wow64->DisableFsRedirection(&previous_state_))
        active_ = true;
      else
        PLOG(WARNING) << "Wow64DisableWow64FSRedirection";
    }
  }

  ~ScopedDisableWow64Redirection() {
    if (active_)
      CHECK(g_wow_64_functions.Get().RevertFsRedirection(previous_state_));
  }

  bool is_active() { return active_; }

 private:
  bool active_;
  PVOID previous_state_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDisableWow64Redirection);
};

// AppliedGPOListProvider implementation that calls actual Windows APIs.
class WinGPOListProvider : public AppliedGPOListProvider {
 public:
  virtual ~WinGPOListProvider() {}

  // AppliedGPOListProvider:
  virtual DWORD GetAppliedGPOList(DWORD flags,
                                  LPCTSTR machine_name,
                                  PSID sid_user,
                                  GUID* extension_guid,
                                  PGROUP_POLICY_OBJECT* gpo_list) OVERRIDE {
    return ::GetAppliedGPOList(flags, machine_name, sid_user, extension_guid,
                               gpo_list);
  }

  virtual BOOL FreeGPOList(PGROUP_POLICY_OBJECT gpo_list) OVERRIDE {
    return ::FreeGPOList(gpo_list);
  }
};

// The default windows GPO list provider used for PolicyLoaderWin.
static base::LazyInstance<WinGPOListProvider> g_win_gpo_list_provider =
    LAZY_INSTANCE_INITIALIZER;

// Returns the entry with key |name| in |dictionary| (can be NULL), or NULL.
const base::DictionaryValue* GetEntry(const base::DictionaryValue* dictionary,
                                      const std::string& name) {
  if (!dictionary)
    return NULL;
  const base::DictionaryValue* entry = NULL;
  dictionary->GetDictionaryWithoutPathExpansion(name, &entry);
  return entry;
}

// Tries to extract the dictionary at |key| in |dict| and returns it.
scoped_ptr<base::DictionaryValue> RemoveDict(base::DictionaryValue* dict,
                                             const std::string& key) {
  base::Value* entry = NULL;
  base::DictionaryValue* result_dict = NULL;
  if (dict && dict->RemoveWithoutPathExpansion(key, &entry) && entry) {
    if (!entry->GetAsDictionary(&result_dict))
      delete entry;
  }

  return make_scoped_ptr(result_dict);
}

std::string GetSchemaTypeForValueType(base::Value::Type value_type) {
  switch (value_type) {
    case base::Value::TYPE_DICTIONARY:
      return json_schema_constants::kObject;
    case base::Value::TYPE_INTEGER:
      return json_schema_constants::kInteger;
    case base::Value::TYPE_LIST:
      return json_schema_constants::kArray;
    case base::Value::TYPE_BOOLEAN:
      return json_schema_constants::kBoolean;
    case base::Value::TYPE_STRING:
      return json_schema_constants::kString;
    default:
      break;
  }

  NOTREACHED() << "Unsupported policy value type " << value_type;
  return json_schema_constants::kNull;
}

// Returns the Value type described in |schema|, or |default_type| if not found.
base::Value::Type GetValueTypeForSchema(const base::DictionaryValue* schema,
                                        base::Value::Type default_type) {
  // JSON-schema types to base::Value::Type mapping.
  static const struct {
    // JSON schema type.
    const char* schema_type;
    // Correspondent value type.
    base::Value::Type value_type;
  } kSchemaToValueTypeMap[] = {
    { schema::kArray,        base::Value::TYPE_LIST        },
    { schema::kBoolean,      base::Value::TYPE_BOOLEAN     },
    { schema::kInteger,      base::Value::TYPE_INTEGER     },
    { schema::kNull,         base::Value::TYPE_NULL        },
    { schema::kNumber,       base::Value::TYPE_DOUBLE      },
    { schema::kObject,       base::Value::TYPE_DICTIONARY  },
    { schema::kString,       base::Value::TYPE_STRING      },
  };

  if (!schema)
    return default_type;
  std::string type;
  if (!schema->GetStringWithoutPathExpansion(schema::kType, &type))
    return default_type;
  for (size_t i = 0; i < arraysize(kSchemaToValueTypeMap); ++i) {
    if (type == kSchemaToValueTypeMap[i].schema_type)
      return kSchemaToValueTypeMap[i].value_type;
  }
  return default_type;
}

// Returns the schema for property |name| given the |schema| of an object.
// Returns the "additionalProperties" schema if no specific schema for
// |name| is present. Returns NULL if no schema is found.
const base::DictionaryValue* GetSchemaFor(const base::DictionaryValue* schema,
                                          const std::string& name) {
  const base::DictionaryValue* properties =
      GetEntry(schema, schema::kProperties);
  const base::DictionaryValue* sub_schema = GetEntry(properties, name);
  if (sub_schema)
    return sub_schema;
  // "additionalProperties" can be a boolean, but that case is ignored.
  return GetEntry(schema, schema::kAdditionalProperties);
}

// Reads the subtree of the Windows registry at |root| into the passed |dict|.
void ReadRegistry(HKEY hive,
                  const string16& root,
                  base::DictionaryValue* dict) {
  // First, read all the values of the key.
  for (RegistryValueIterator it(hive, root.c_str()); it.Valid(); ++it) {
    const std::string name = UTF16ToUTF8(it.Name());
    switch (it.Type()) {
      case REG_SZ:
      case REG_EXPAND_SZ:
        dict->SetStringWithoutPathExpansion(name, UTF16ToUTF8(it.Value()));
        continue;
      case REG_DWORD_LITTLE_ENDIAN:
      case REG_DWORD_BIG_ENDIAN:
        if (it.ValueSize() == sizeof(DWORD)) {
          DWORD dword_value = *(reinterpret_cast<const DWORD*>(it.Value()));
          if (it.Type() == REG_DWORD_BIG_ENDIAN)
            dword_value = base::NetToHost32(dword_value);
          else
            dword_value = base::ByteSwapToLE32(dword_value);
          dict->SetIntegerWithoutPathExpansion(name, dword_value);
          continue;
        }
      case REG_NONE:
      case REG_LINK:
      case REG_MULTI_SZ:
      case REG_RESOURCE_LIST:
      case REG_FULL_RESOURCE_DESCRIPTOR:
      case REG_RESOURCE_REQUIREMENTS_LIST:
      case REG_QWORD_LITTLE_ENDIAN:
        // Unsupported type, message gets logged below.
        break;
    }

    LOG(WARNING) << "Failed to read hive " << hive << " at "
                 << root << "\\" << name
                 << " type " << it.Type();
  }

  // Recurse for all subkeys.
  for (RegistryKeyIterator it(hive, root.c_str()); it.Valid(); ++it) {
    std::string name(UTF16ToUTF8(it.Name()));
    if (dict->HasKey(name)) {
      DLOG(WARNING) << "Ignoring registry key because a value exists with the "
                       "same name: " << root << "\\" << name;
    } else {
      scoped_ptr<base::DictionaryValue> subdict(new base::DictionaryValue());
      ReadRegistry(hive, root + L"\\" + it.Name(), subdict.get());
      dict->SetWithoutPathExpansion(name, subdict.release());
    }
  }
}

// Converts |value| in raw GPO representation to the internal policy value, as
// described by |schema|. This maps the ambiguous GPO data types to the
// internal policy value representations.
scoped_ptr<base::Value> ConvertPolicyValue(
    const base::Value& value,
    const base::DictionaryValue* schema) {
  // Figure out the type to convert to from the schema.
  const base::Value::Type result_type(
      GetValueTypeForSchema(schema, value.GetType()));

  // If the type is good already, go with it.
  if (value.IsType(result_type)) {
    // Recurse for complex types if there is a schema.
    if (schema) {
      const base::DictionaryValue* dict = NULL;
      const base::ListValue* list = NULL;
      if (value.GetAsDictionary(&dict)) {
        scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
        for (base::DictionaryValue::Iterator entry(*dict); !entry.IsAtEnd();
             entry.Advance()) {
          scoped_ptr<base::Value> converted_value(
              ConvertPolicyValue(entry.value(),
                                 GetSchemaFor(schema, entry.key())));
          result->SetWithoutPathExpansion(entry.key(),
                                          converted_value.release());
        }
        return result.Pass();
      } else if (value.GetAsList(&list)) {
        scoped_ptr<base::ListValue> result(new base::ListValue());
        const base::DictionaryValue* item_schema =
            GetEntry(schema, schema::kItems);
        for (base::ListValue::const_iterator entry(list->begin());
             entry != list->end(); ++entry) {
          result->Append(ConvertPolicyValue(**entry, item_schema).release());
        }
        return result.Pass();
      }
    }
    return make_scoped_ptr(value.DeepCopy());
  }

  // Else, do some conversions to map windows registry data types to JSON types.
  std::string string_value;
  int int_value = 0;
  switch (result_type) {
    case base::Value::TYPE_NULL: {
      return make_scoped_ptr(base::Value::CreateNullValue());
    }
    case base::Value::TYPE_BOOLEAN: {
      // Accept booleans encoded as either string or integer.
      if (value.GetAsInteger(&int_value) ||
          (value.GetAsString(&string_value) &&
           base::StringToInt(string_value, &int_value))) {
        return make_scoped_ptr(Value::CreateBooleanValue(int_value != 0));
      }
      break;
    }
    case base::Value::TYPE_INTEGER: {
      // Integers may be string-encoded.
      if (value.GetAsString(&string_value) &&
          base::StringToInt(string_value, &int_value)) {
        return make_scoped_ptr(base::Value::CreateIntegerValue(int_value));
      }
      break;
    }
    case base::Value::TYPE_DOUBLE: {
      // Doubles may be string-encoded or integer-encoded.
      double double_value = 0;
      if (value.GetAsInteger(&int_value)) {
        return make_scoped_ptr(base::Value::CreateDoubleValue(int_value));
      } else if (value.GetAsString(&string_value) &&
                 base::StringToDouble(string_value, &double_value)) {
        return make_scoped_ptr(base::Value::CreateDoubleValue(double_value));
      }
      break;
    }
    case base::Value::TYPE_LIST: {
      // Lists are encoded as subkeys with numbered value in the registry.
      const base::DictionaryValue* dict = NULL;
      if (value.GetAsDictionary(&dict)) {
        scoped_ptr<base::ListValue> result(new base::ListValue());
        const base::DictionaryValue* item_schema =
            GetEntry(schema, schema::kItems);
        for (int i = 1; ; ++i) {
          const base::Value* entry = NULL;
          if (!dict->Get(base::IntToString(i), &entry))
            break;
          result->Append(ConvertPolicyValue(*entry, item_schema).release());
        }
        return result.Pass();
      }
      // Fall through in order to accept lists encoded as JSON strings.
    }
    case base::Value::TYPE_DICTIONARY: {
      // Dictionaries may be encoded as JSON strings.
      if (value.GetAsString(&string_value)) {
        scoped_ptr<base::Value> result(base::JSONReader::Read(string_value));
        if (result && result->IsType(result_type))
          return result.Pass();
      }
      break;
    }
    case base::Value::TYPE_STRING:
    case base::Value::TYPE_BINARY:
      // No conversion possible.
      break;
  }

  LOG(WARNING) << "Failed to convert " << value.GetType()
               << " to " << result_type;
  return make_scoped_ptr(base::Value::CreateNullValue());
}

// Parses |gpo_dict| according to |schema| and writes the resulting policy
// settings to |policy| for the given |scope| and |level|.
void ParsePolicy(const base::DictionaryValue* gpo_dict,
                 PolicyLevel level,
                 PolicyScope scope,
                 const base::DictionaryValue* schema,
                 PolicyMap* policy) {
  if (!gpo_dict)
    return;

  scoped_ptr<base::Value> policy_value(ConvertPolicyValue(*gpo_dict, schema));
  const base::DictionaryValue* policy_dict = NULL;
  if (!policy_value->GetAsDictionary(&policy_dict) || !policy_dict) {
    LOG(WARNING) << "Root policy object is not a dictionary!";
    return;
  }

  policy->LoadFrom(policy_dict, level, scope);
}

}  // namespace

const base::FilePath::CharType PolicyLoaderWin::kPRegFileName[] =
    FILE_PATH_LITERAL("Registry.pol");

PolicyLoaderWin::PolicyLoaderWin(const PolicyDefinitionList* policy_list,
                                 const string16& chrome_policy_key,
                                 AppliedGPOListProvider* gpo_provider)
    : is_initialized_(false),
      policy_list_(policy_list),
      chrome_policy_key_(chrome_policy_key),
      gpo_provider_(gpo_provider),
      user_policy_changed_event_(false, false),
      machine_policy_changed_event_(false, false),
      user_policy_watcher_failed_(false),
      machine_policy_watcher_failed_(false) {
  if (!RegisterGPNotification(user_policy_changed_event_.handle(), false)) {
    DPLOG(WARNING) << "Failed to register user group policy notification";
    user_policy_watcher_failed_ = true;
  }
  if (!RegisterGPNotification(machine_policy_changed_event_.handle(), true)) {
    DPLOG(WARNING) << "Failed to register machine group policy notification.";
    machine_policy_watcher_failed_ = true;
  }
}

PolicyLoaderWin::~PolicyLoaderWin() {
  user_policy_watcher_.StopWatching();
  machine_policy_watcher_.StopWatching();
}

// static
scoped_ptr<PolicyLoaderWin> PolicyLoaderWin::Create(
    const PolicyDefinitionList* policy_list) {
  return make_scoped_ptr(
      new PolicyLoaderWin(policy_list, kRegistryChromePolicyKey,
                          g_win_gpo_list_provider.Pointer()));
}

void PolicyLoaderWin::InitOnFile() {
  is_initialized_ = true;
  SetupWatches();
}

scoped_ptr<PolicyBundle> PolicyLoaderWin::Load() {
  // Reset the watches BEFORE reading the individual policies to avoid
  // missing a change notification.
  if (is_initialized_)
    SetupWatches();

  if (chrome_policy_schema_.empty())
    BuildChromePolicySchema();

  // Policy scope and corresponding hive.
  static const struct {
    PolicyScope scope;
    HKEY hive;
  } kScopes[] = {
    { POLICY_SCOPE_MACHINE, HKEY_LOCAL_MACHINE },
    { POLICY_SCOPE_USER,    HKEY_CURRENT_USER  },
  };

  // Load policy data for the different scopes/levels and merge them.
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  PolicyMap* chrome_policy =
      &bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  for (size_t i = 0; i < arraysize(kScopes); ++i) {
    PolicyScope scope = kScopes[i].scope;
    PolicyLoadStatusSample status;
    base::DictionaryValue gpo_dict;

    HANDLE policy_lock =
        EnterCriticalPolicySection(scope == POLICY_SCOPE_MACHINE);
    if (policy_lock == NULL)
      PLOG(ERROR) << "EnterCriticalPolicySection";

    if (!ReadPolicyFromGPO(scope, &gpo_dict, &status)) {
      VLOG(1) << "Failed to read GPO files for " << scope
              << " falling back to registry.";
      ReadRegistry(kScopes[i].hive, chrome_policy_key_, &gpo_dict);
    }

    if (!LeaveCriticalPolicySection(policy_lock))
      PLOG(ERROR) << "LeaveCriticalPolicySection";

    // Remove special-cased entries from the GPO dictionary.
    base::DictionaryValue* temp_dict = NULL;
    scoped_ptr<base::DictionaryValue> recommended_dict(
        RemoveDict(&gpo_dict, kKeyRecommended));
    scoped_ptr<base::DictionaryValue> third_party_dict(
        RemoveDict(&gpo_dict, kKeyThirdParty));

    // Load Chrome policy.
    LoadChromePolicy(&gpo_dict, POLICY_LEVEL_MANDATORY, scope, chrome_policy);
    LoadChromePolicy(recommended_dict.get(), POLICY_LEVEL_RECOMMENDED, scope,
                     chrome_policy);

    // Load 3rd-party policy.
    if (third_party_dict)
      Load3rdPartyPolicy(third_party_dict.get(), scope, bundle.get());
  }

  return bundle.Pass();
}

void PolicyLoaderWin::BuildChromePolicySchema() {
  scoped_ptr<base::DictionaryValue> properties(new base::DictionaryValue());
  for (const PolicyDefinitionList::Entry* e = policy_list_->begin;
       e != policy_list_->end; ++e) {
    const std::string schema_type = GetSchemaTypeForValueType(e->value_type);
    scoped_ptr<base::DictionaryValue> entry_schema(new base::DictionaryValue());
    entry_schema->SetStringWithoutPathExpansion(json_schema_constants::kType,
                                                schema_type);

    if (e->value_type == base::Value::TYPE_LIST) {
      scoped_ptr<base::DictionaryValue> items_schema(
          new base::DictionaryValue());
      items_schema->SetStringWithoutPathExpansion(
          json_schema_constants::kType, json_schema_constants::kString);
      entry_schema->SetWithoutPathExpansion(json_schema_constants::kItems,
                                            items_schema.release());
    }
    properties->SetWithoutPathExpansion(e->name, entry_schema.release());
  }
  chrome_policy_schema_.SetStringWithoutPathExpansion(
      json_schema_constants::kType, json_schema_constants::kObject);
  chrome_policy_schema_.SetWithoutPathExpansion(
      json_schema_constants::kProperties, properties.release());
}

bool PolicyLoaderWin::ReadPRegFile(const base::FilePath& preg_file,
                                   base::DictionaryValue* policy,
                                   PolicyLoadStatusSample* status) {
  // The following deals with the minor annoyance that Wow64 FS redirection
  // might need to be turned off: This is the case if running as a 32-bit
  // process on a 64-bit system, in which case Wow64 FS redirection redirects
  // access to the %WINDIR%/System32/GroupPolicy directory to
  // %WINDIR%/SysWOW64/GroupPolicy, but the file is actually in the
  // system-native directory.
  if (file_util::PathExists(preg_file)) {
    return preg_parser::ReadFile(preg_file, chrome_policy_key_, policy, status);
  } else {
    // Try with redirection switched off.
    ScopedDisableWow64Redirection redirection_disable;
    if (redirection_disable.is_active() && file_util::PathExists(preg_file)) {
      status->Add(POLICY_LOAD_STATUS_WOW64_REDIRECTION_DISABLED);
      return preg_parser::ReadFile(preg_file, chrome_policy_key_, policy,
                                   status);
    }
  }

  // Report the error.
  LOG(ERROR) << "PReg file doesn't exist: " << preg_file.value();
  status->Add(POLICY_LOAD_STATUS_MISSING);
  return false;
}

bool PolicyLoaderWin::LoadGPOPolicy(PolicyScope scope,
                                    PGROUP_POLICY_OBJECT policy_object_list,
                                    base::DictionaryValue* policy,
                                    PolicyLoadStatusSample* status) {
  base::DictionaryValue parsed_policy;
  base::DictionaryValue forced_policy;
  for (GROUP_POLICY_OBJECT* policy_object = policy_object_list;
       policy_object; policy_object = policy_object->pNext) {
    if (policy_object->dwOptions & GPO_FLAG_DISABLE)
      continue;

    if (PathIsUNC(policy_object->lpFileSysPath)) {
      // UNC path: Assume this is an AD-managed machine, which updates the
      // registry via GPO's standard registry CSE periodically. Fall back to
      // reading from the registry in this case.
      status->Add(POLICY_LOAD_STATUS_INACCCESSIBLE);
      return false;
    }

    base::FilePath preg_file_path(
        base::FilePath(policy_object->lpFileSysPath).Append(kPRegFileName));
    if (policy_object->dwOptions & GPO_FLAG_FORCE) {
      base::DictionaryValue new_forced_policy;
      if (!ReadPRegFile(preg_file_path, &new_forced_policy, status))
        return false;

      // Merge with existing forced policy, giving precedence to the existing
      // forced policy.
      new_forced_policy.MergeDictionary(&forced_policy);
      forced_policy.Swap(&new_forced_policy);
    } else {
      if (!ReadPRegFile(preg_file_path, &parsed_policy, status))
        return false;
    }
  }

  // Merge, give precedence to forced policy.
  parsed_policy.MergeDictionary(&forced_policy);
  policy->Swap(&parsed_policy);

  return true;
}


bool PolicyLoaderWin::ReadPolicyFromGPO(PolicyScope scope,
                                        base::DictionaryValue* policy,
                                        PolicyLoadStatusSample* status) {
  PGROUP_POLICY_OBJECT policy_object_list = NULL;
  DWORD flags = scope == POLICY_SCOPE_MACHINE ? GPO_LIST_FLAG_MACHINE : 0;
  if (gpo_provider_->GetAppliedGPOList(
          flags, NULL, NULL, &kRegistrySettingsCSEGUID,
          &policy_object_list) != ERROR_SUCCESS) {
    PLOG(ERROR) << "GetAppliedGPOList scope " << scope;
    status->Add(POLICY_LOAD_STATUS_QUERY_FAILED);
    return false;
  }

  bool result = true;
  if (policy_object_list) {
    result = LoadGPOPolicy(scope, policy_object_list, policy, status);
    if (!gpo_provider_->FreeGPOList(policy_object_list))
      LOG(WARNING) << "FreeGPOList";
  } else {
    status->Add(POLICY_LOAD_STATUS_NO_POLICY);
  }

  return result;
}

void PolicyLoaderWin::LoadChromePolicy(const base::DictionaryValue* gpo_dict,
                                       PolicyLevel level,
                                       PolicyScope scope,
                                       PolicyMap* chrome_policy_map) {
  PolicyMap policy;
  ParsePolicy(gpo_dict, level, scope, &chrome_policy_schema_, &policy);
  chrome_policy_map->MergeFrom(policy);
}

void PolicyLoaderWin::Load3rdPartyPolicy(
    const DictionaryValue* gpo_dict,
    PolicyScope scope,
    PolicyBundle* bundle) {
  // Map of known 3rd party policy domain name to their enum values.
  static const struct {
    const char* name;
    PolicyDomain domain;
  } k3rdPartyDomains[] = {
    { "extensions", POLICY_DOMAIN_EXTENSIONS },
    // Support a common misspelling. The correct spelling is first, so it takes
    // precedence in case of collisions.
    { "Extensions", POLICY_DOMAIN_EXTENSIONS },
  };

  // Policy level and corresponding path.
  static const struct {
    PolicyLevel level;
    const char* path;
  } kLevels[] = {
    { POLICY_LEVEL_MANDATORY,   kKeyMandatory   },
    { POLICY_LEVEL_RECOMMENDED, kKeyRecommended },
  };

  for (size_t i = 0; i < arraysize(k3rdPartyDomains); i++) {
    const char* name = k3rdPartyDomains[i].name;
    const PolicyDomain domain = k3rdPartyDomains[i].domain;
    const base::DictionaryValue* domain_dict = NULL;
    if (!gpo_dict->GetDictionaryWithoutPathExpansion(name, &domain_dict) ||
        !domain_dict) {
      continue;
    }

    for (base::DictionaryValue::Iterator component(*domain_dict);
         !component.IsAtEnd(); component.Advance()) {
      const base::DictionaryValue* component_dict = NULL;
      if (!component.value().GetAsDictionary(&component_dict) ||
          !component_dict) {
        continue;
      }

      // Load the schema.
      scoped_ptr<base::Value> schema;
      const base::DictionaryValue* schema_dict = NULL;
      std::string schema_json;
      if (component_dict->GetStringWithoutPathExpansion(kKeySchema,
                                                        &schema_json)) {
        schema.reset(base::JSONReader::Read(schema_json));
        if (!schema || !schema->GetAsDictionary(&schema_dict)) {
          LOG(WARNING) << "Failed to parse 3rd-part policy schema for "
                       << domain << "/" << component.key();
        }
      }

      // Parse policy.
      for (size_t j = 0; j < arraysize(kLevels); j++) {
        const base::DictionaryValue* policy_dict = NULL;
        if (!component_dict->GetDictionaryWithoutPathExpansion(
                kLevels[j].path, &policy_dict) ||
            !policy_dict) {
          continue;
        }

        PolicyMap policy;
        ParsePolicy(policy_dict, kLevels[j].level, scope, schema_dict, &policy);
        PolicyNamespace policy_namespace(domain, component.key());
        bundle->Get(policy_namespace).MergeFrom(policy);
      }
    }
  }
}

void PolicyLoaderWin::SetupWatches() {
  DCHECK(is_initialized_);
  if (!user_policy_watcher_failed_ &&
      !user_policy_watcher_.GetWatchedObject() &&
      !user_policy_watcher_.StartWatching(
          user_policy_changed_event_.handle(), this)) {
    DLOG(WARNING) << "Failed to start watch for user policy change event";
    user_policy_watcher_failed_ = true;
  }
  if (!machine_policy_watcher_failed_ &&
      !machine_policy_watcher_.GetWatchedObject() &&
      !machine_policy_watcher_.StartWatching(
          machine_policy_changed_event_.handle(), this)) {
    DLOG(WARNING) << "Failed to start watch for machine policy change event";
    machine_policy_watcher_failed_ = true;
  }
}

void PolicyLoaderWin::OnObjectSignaled(HANDLE object) {
  DCHECK(object == user_policy_changed_event_.handle() ||
         object == machine_policy_changed_event_.handle())
      << "unexpected object signaled policy reload, obj = "
      << std::showbase << std::hex << object;
  Reload(false);
}

}  // namespace policy
