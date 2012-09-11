// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_loader_win.h"

#include <string>

#include <string.h>

#include <userenv.h>

// userenv.dll is required for RegisterGPNotification().
#pragma comment(lib, "userenv.lib")

#include "base/basictypes.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"

using base::win::RegKey;
using base::win::RegistryKeyIterator;
using base::win::RegistryValueIterator;
using namespace policy::registry_constants;

namespace policy {

namespace registry_constants {
  const wchar_t kPathSep[] = L"\\";
  const wchar_t kThirdParty[] = L"3rdparty";
  const wchar_t kMandatory[] = L"policy";
  const wchar_t kRecommended[] = L"recommended";
  const wchar_t kSchema[] = L"schema";
  const char kType[] = "type";
  const char kProperties[] = "properties";
  const char kAdditionalProperties[] = "additionalProperties";
  const char kItems[] = "items";
}  // namespace registry_constants

namespace {

// Map of registry hives to their corresponding policy scope, in decreasing
// order of priority.
const struct {
  HKEY hive;
  PolicyScope scope;
} kHives[] = {
  { HKEY_LOCAL_MACHINE,   POLICY_SCOPE_MACHINE  },
  { HKEY_CURRENT_USER,    POLICY_SCOPE_USER     },
};

// Determines the registry key with the highest priority that contains
// the |key_path| key with a |value_name| value inside.
// |key_path| is a suffix to the Chrome mandatory or recommended registry key,
// and can be empty to lookup values directly at that key.
// |value_name| is the name of a value that should exist at that path. If it
// is empty, then only the existence of the path is verified.
// Returns true if |key| was updated to point to a key with a |key_path| suffix
// and optional |value_name| value inside. In that case, |level| and |scope|
// will contain the appropriate values for the key found.
// Returns false otherwise.
bool LoadHighestPriorityKey(const string16& key_path,
                            const string16& value_name,
                            RegKey* key,
                            PolicyLevel* level,
                            PolicyScope* scope) {
  // |kKeyPaths| is in decreasing order of priority.
  static const struct {
    const wchar_t* path;
    PolicyLevel level;
  } kKeyPaths[] = {
    { kRegistryMandatorySubKey,   POLICY_LEVEL_MANDATORY    },
    { kRegistryRecommendedSubKey, POLICY_LEVEL_RECOMMENDED  },
  };

  // Lookup at the mandatory path for both user and machine policies first, and
  // then at the recommended path.
  for (size_t k = 0; k < arraysize(kKeyPaths); ++k) {
    for (size_t h = 0; h < arraysize(kHives); ++h) {
      string16 path(kKeyPaths[k].path);
      if (!key_path.empty())
        path += kPathSep + key_path;
      if (key->Open(kHives[h].hive, path.c_str(), KEY_READ) != ERROR_SUCCESS ||
          !key->Valid()) {
        continue;
      }
      if (value_name.empty() || key->HasValue(value_name.c_str())) {
        *level = kKeyPaths[k].level;
        *scope = kHives[h].scope;
        return true;
      }
    }
  }
  return false;
}

// Reads a REG_SZ string at |key| named |name| into |result|. Returns false if
// the string could not be read.
bool ReadRegistryString(RegKey* key,
                        const string16& name,
                        string16* result) {
  DWORD value_size = 0;
  DWORD key_type = 0;
  scoped_array<uint8> buffer;

  if (key->ReadValue(name.c_str(), 0, &value_size, &key_type) != ERROR_SUCCESS)
    return false;
  if (key_type != REG_SZ)
    return false;

  // According to the Microsoft documentation, the string
  // buffer may not be explicitly 0-terminated. Allocate a
  // slightly larger buffer and pre-fill to zeros to guarantee
  // the 0-termination.
  buffer.reset(new uint8[value_size + 2]);
  memset(buffer.get(), 0, value_size + 2);
  key->ReadValue(name.c_str(), buffer.get(), &value_size, NULL);
  result->assign(reinterpret_cast<const wchar_t*>(buffer.get()));
  return true;
}

// Reads a REG_DWORD integer at |key| named |name| into |result|. Returns false
// if the value could no be read.
bool ReadRegistryInteger(RegKey* key,
                         const string16& name,
                         uint32* result) {
  DWORD dword;
  if (key->ReadValueDW(name.c_str(), &dword) != ERROR_SUCCESS)
    return false;
  *result = dword;
  return true;
}

// Returns the Value for a Chrome string policy named |name|, or NULL if
// it wasn't found. The caller owns the returned value.
base::Value* ReadChromeStringValue(const string16& name,
                                   PolicyLevel* level,
                                   PolicyScope* scope) {
  RegKey key;
  if (!LoadHighestPriorityKey(string16(), name, &key, level, scope))
    return NULL;
  string16 value;
  if (!ReadRegistryString(&key, name, &value))
    return NULL;
  return base::Value::CreateStringValue(value);
}

// Returns the Value for a Chrome string list policy named |name|,
// or NULL if it wasn't found. The caller owns the returned value.
base::Value* ReadChromeStringListValue(const string16& name,
                                       PolicyLevel* level,
                                       PolicyScope* scope) {
  RegKey key;
  if (!LoadHighestPriorityKey(name, string16(), &key, level, scope))
    return NULL;
  base::ListValue* result = new base::ListValue();
  string16 value;
  int index = 0;
  while (ReadRegistryString(&key, base::IntToString16(++index), &value))
    result->Append(base::Value::CreateStringValue(value));
  return result;
}

// Returns the Value for a Chrome boolean policy named |name|,
// or NULL if it wasn't found. The caller owns the returned value.
base::Value* ReadChromeBooleanValue(const string16& name,
                                    PolicyLevel* level,
                                    PolicyScope* scope) {
  RegKey key;
  if (!LoadHighestPriorityKey(string16(), name, &key, level, scope))
    return NULL;
  uint32 value;
  if (!ReadRegistryInteger(&key, name, &value))
    return NULL;
  return base::Value::CreateBooleanValue(value != 0u);
}

// Returns the Value for a Chrome integer policy named |name|,
// or NULL if it wasn't found. The caller owns the returned value.
base::Value* ReadChromeIntegerValue(const string16& name,
                                    PolicyLevel* level,
                                    PolicyScope* scope) {
  RegKey key;
  if (!LoadHighestPriorityKey(string16(), name, &key, level, scope))
    return NULL;
  uint32 value;
  if (!ReadRegistryInteger(&key, name, &value))
    return NULL;
  return base::Value::CreateIntegerValue(value);
}

// Returns the Value for a Chrome dictionary policy named |name|,
// or NULL if it wasn't found. The caller owns the returned value.
base::Value* ReadChromeDictionaryValue(const string16& name,
                                       PolicyLevel* level,
                                       PolicyScope* scope) {
  // Dictionaries are encoded as JSON strings on Windows.
  //
  // A dictionary could be stored as a subkey, with each of its entries
  // within that subkey. However, it would be impossible to recover the
  // type for some of those entries:
  //  - Booleans are stored as DWORDS and are indistinguishable from
  //    integers;
  //  - Lists are stored as a subkey, with entries mapping 0 to N-1 to
  //    their value. This is indistinguishable from a Dictionary with
  //    integer keys.
  //
  // The GPO policy editor also has a limited data entry form that doesn't
  // support dictionaries.
  RegKey key;
  if (!LoadHighestPriorityKey(string16(), name, &key, level, scope))
    return NULL;
  string16 value;
  if (!ReadRegistryString(&key, name, &value))
    return NULL;
  return base::JSONReader::Read(UTF16ToUTF8(value));
}

// Returns the Value type described in |schema|, or |default_type| if not found.
base::Value::Type GetType(const base::DictionaryValue* schema,
                          base::Value::Type default_type) {
  // JSON-schema types to base::Value::Type mapping.
  static const struct {
    // JSON schema type.
    const char* schema_type;
    // Correspondent value type.
    base::Value::Type value_type;
  } kSchemaToValueTypeMap[] = {
    { "array",        base::Value::TYPE_LIST        },
    { "boolean",      base::Value::TYPE_BOOLEAN     },
    { "integer",      base::Value::TYPE_INTEGER     },
    { "null",         base::Value::TYPE_NULL        },
    { "number",       base::Value::TYPE_DOUBLE      },
    { "object",       base::Value::TYPE_DICTIONARY  },
    { "string",       base::Value::TYPE_STRING      },
  };

  if (!schema)
    return default_type;
  std::string type;
  if (!schema->GetString(kType, &type))
    return default_type;
  for (size_t i = 0; i < arraysize(kSchemaToValueTypeMap); ++i) {
    if (type == kSchemaToValueTypeMap[i].schema_type)
      return kSchemaToValueTypeMap[i].value_type;
  }
  return default_type;
}

// Returns the default type for registry entries of |reg_type|, when there is
// no schema defined type for a policy.
base::Value::Type GetDefaultFor(DWORD reg_type) {
  return reg_type == REG_DWORD ? base::Value::TYPE_INTEGER :
                                 base::Value::TYPE_STRING;
}

// Returns the entry with key |name| in |dictionary| (can be NULL), or NULL.
const base::DictionaryValue* GetEntry(const base::DictionaryValue* dictionary,
                                const std::string& name) {
  if (!dictionary)
    return NULL;
  const base::DictionaryValue* entry = NULL;
  dictionary->GetDictionary(name, &entry);
  return entry;
}

// Returns the schema for property |name| given the |schema| of an object.
// Returns the "additionalProperties" schema if no specific schema for
// |name| is present. Returns NULL if no schema is found.
const base::DictionaryValue* GetSchemaFor(const base::DictionaryValue* schema,
                                    const std::string& name) {
  const base::DictionaryValue* properties = GetEntry(schema, kProperties);
  const base::DictionaryValue* sub_schema = GetEntry(properties, name);
  if (sub_schema)
    return sub_schema;
  // "additionalProperties" can be a boolean, but that case is ignored.
  return GetEntry(schema, kAdditionalProperties);
}

// Converts string |value| to another |type|, if possible.
base::Value* ConvertComponentStringValue(const string16& value,
                                         base::Value::Type type) {
  switch (type) {
    case base::Value::TYPE_NULL:
      return base::Value::CreateNullValue();

    case base::Value::TYPE_BOOLEAN: {
      int int_value;
      if (base::StringToInt(value, &int_value))
        return base::Value::CreateBooleanValue(int_value != 0);
      return NULL;
    }

    case base::Value::TYPE_INTEGER: {
      int int_value;
      if (base::StringToInt(value, &int_value))
          return base::Value::CreateIntegerValue(int_value);
      return NULL;
    }

    case base::Value::TYPE_DOUBLE: {
      double double_value;
      if (base::StringToDouble(UTF16ToUTF8(value), &double_value))
        return base::Value::CreateDoubleValue(double_value);
      DLOG(WARNING) << "Failed to read policy value as double: " << value;
      return NULL;
    }

    case base::Value::TYPE_STRING:
      return base::Value::CreateStringValue(value);

    case base::Value::TYPE_DICTIONARY:
    case base::Value::TYPE_LIST:
      return base::JSONReader::Read(UTF16ToUTF8(value));

    case base::Value::TYPE_BINARY:
      DLOG(WARNING) << "Cannot convert REG_SZ entry to type " << type;
      return NULL;
  }
  NOTREACHED();
  return NULL;
}

// Converts an integer |value| to another |type|, if possible.
base::Value* ConvertComponentIntegerValue(uint32 value,
                                          base::Value::Type type) {
  switch (type) {
    case base::Value::TYPE_BOOLEAN:
      return base::Value::CreateBooleanValue(value != 0);

    case base::Value::TYPE_INTEGER:
      return base::Value::CreateIntegerValue(value);

    case base::Value::TYPE_DOUBLE:
      return base::Value::CreateDoubleValue(value);

    case base::Value::TYPE_NULL:
    case base::Value::TYPE_STRING:
    case base::Value::TYPE_BINARY:
    case base::Value::TYPE_DICTIONARY:
    case base::Value::TYPE_LIST:
      DLOG(WARNING) << "Cannot convert REG_DWORD entry to type " << type;
      return NULL;
  }
  NOTREACHED();
  return NULL;
}

// Reads a simple (non-Dictionary, non-Array) value from the registry |key|
// named |name| with registry type |reg_type| as a value of type |type|.
// Returns NULL if the value could not be loaded or converted.
base::Value* ReadComponentSimpleValue(RegKey* key,
                                      const string16& name,
                                      DWORD reg_type,
                                      base::Value::Type type) {
  switch (reg_type) {
    case REG_SZ: {
      string16 value;
      if (ReadRegistryString(key, name, &value))
        return ConvertComponentStringValue(value, type);
      break;
    }

    case REG_DWORD: {
      uint32 value;
      if (ReadRegistryInteger(key, name, &value))
        return ConvertComponentIntegerValue(value, type);
      break;
    }

    default:
      DLOG(WARNING) << "Registry type not supported for key " << name;
      break;
  }
  NOTREACHED();
  return NULL;
}

// Forward declaration for ReadComponentListValue().
base::DictionaryValue* ReadComponentDictionaryValue(
    HKEY hive,
    const string16& path,
    const base::DictionaryValue* schema);

// Loads the list at |path| in the given |hive|. |schema| is a JSON schema
// (http://json-schema.org/) that describes the expected type of the list.
// Ownership of the result is transferred to the caller.
base::ListValue* ReadComponentListValue(HKEY hive,
                                        const string16& path,
                                        const base::DictionaryValue* schema) {
  // The sub-elements are indexed from 1 to N. They can be represented as
  // registry values or registry keys though; use |schema| first to try to
  // determine the right type, and if that fails default to STRING.

  RegKey key(hive, path.c_str(), KEY_READ);
  if (!key.Valid())
    return NULL;

  // Get the schema for list items.
  schema = GetEntry(schema, kItems);
  base::Value::Type type = GetType(schema, base::Value::TYPE_STRING);
  base::ListValue* list = new base::ListValue();
  for (int i = 1; ; ++i) {
    string16 name = base::IntToString16(i);
    base::Value* value = NULL;
    if (type == base::Value::TYPE_DICTIONARY) {
      value =
          ReadComponentDictionaryValue(hive, path + kPathSep + name, schema);
    } else if (type == base::Value::TYPE_LIST) {
      value = ReadComponentListValue(hive, path + kPathSep + name, schema);
    } else {
      DWORD reg_type;
      key.ReadValue(name.c_str(), NULL, NULL, &reg_type);
      if (reg_type != REG_NONE)
        value = ReadComponentSimpleValue(&key, name, reg_type, type);
    }
    if (value)
      list->Append(value);
    else
      break;
  }
  return list;
}

// Loads the dictionary at |path| in the given |hive|. |schema| is a JSON
// schema (http://json-schema.org/) that describes the expected types for the
// dictionary entries. When the type for a certain entry isn't described in the
// schema, a default conversion takes place. |schema| can be NULL.
// Ownership of the result is transferred to the caller.
base::DictionaryValue* ReadComponentDictionaryValue(
    HKEY hive,
    const string16& path,
    const base::DictionaryValue* schema) {
  // A "value" in the registry is like a file in a filesystem, and a "key" is
  // like a directory, that contains other "values" and "keys".
  // Unfortunately it is possible to have a name both as a "value" and a "key".
  // In those cases, the sub "key" will be ignored; this choice is arbitrary.

  // First iterate over all the "values" in |path| and convert them; then
  // recurse into each "key" in |path| and convert them as dictionaries.

  RegKey key(hive, path.c_str(), KEY_READ);
  if (!key.Valid())
    return NULL;

  base::DictionaryValue* dict = new base::DictionaryValue();
  for (RegistryValueIterator it(hive, path.c_str()); it.Valid(); ++it) {
    string16 name16(it.Name());
    std::string name(UTF16ToUTF8(name16));
    const base::DictionaryValue* sub_schema = GetSchemaFor(schema, name);
    base::Value::Type type = GetType(sub_schema, GetDefaultFor(it.Type()));
    base::Value* value =
        ReadComponentSimpleValue(&key, name16, it.Type(), type);
    if (value)
      dict->Set(name, value);
  }

  for (RegistryKeyIterator it(hive, path.c_str()); it.Valid(); ++it) {
    string16 name16(it.Name());
    std::string name(UTF16ToUTF8(name16));
    if (dict->HasKey(name)) {
      DLOG(WARNING) << "Ignoring registry key because a value exists with the "
                       "same name: " << path << kPathSep << name;
      continue;
    }

    const base::DictionaryValue* sub_schema = GetSchemaFor(schema, name);
    base::Value::Type type = GetType(sub_schema, base::Value::TYPE_DICTIONARY);
    base::Value* value = NULL;
    const string16 sub_path = path + kPathSep + name16;
    if (type == base::Value::TYPE_DICTIONARY) {
      value = ReadComponentDictionaryValue(hive, sub_path, sub_schema);
    } else if (type == base::Value::TYPE_LIST) {
      value = ReadComponentListValue(hive, sub_path, sub_schema);
    } else {
      DLOG(WARNING) << "Can't read a simple type in registry key at " << path;
    }
    if (value)
      dict->Set(name, value);
  }

  return dict;
}

// Reads a JSON schema from the given |registry_value|, at the given
// |registry_key| in |hive|. |registry_value| must be a string (REG_SZ), and
// is decoded as JSON data. Returns NULL on failure. Ownership is transferred
// to the caller.
base::DictionaryValue* ReadRegistrySchema(HKEY hive,
                                          const string16& registry_key,
                                          const string16& registry_value) {
  RegKey key(hive, registry_key.c_str(), KEY_READ);
  string16 schema;
  if (!ReadRegistryString(&key, registry_value, &schema))
    return NULL;
  // A JSON schema is represented in JSON too.
  scoped_ptr<base::Value> value(base::JSONReader::Read(UTF16ToUTF8(schema)));
  if (!value.get())
    return NULL;
  base::DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict))
    return NULL;
  // The top-level entry must be an object, and each of its properties maps
  // a policy name to its schema.
  if (GetType(dict, base::Value::TYPE_DICTIONARY) !=
      base::Value::TYPE_DICTIONARY) {
    DLOG(WARNING) << "schema top-level type isn't \"object\"";
    return NULL;
  }
  value.release();
  return dict;
}

}  // namespace

PolicyLoaderWin::PolicyLoaderWin(const PolicyDefinitionList* policy_list)
    : is_initialized_(false),
      policy_list_(policy_list),
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

void PolicyLoaderWin::InitOnFile() {
  is_initialized_ = true;
  SetupWatches();
}

scoped_ptr<PolicyBundle> PolicyLoaderWin::Load() {
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  LoadChromePolicy(&bundle->Get(POLICY_DOMAIN_CHROME, std::string()));
  Load3rdPartyPolicies(bundle.get());
  return bundle.Pass();
}

void PolicyLoaderWin::LoadChromePolicy(PolicyMap* chrome_policies) {
  // Reset the watches BEFORE reading the individual policies to avoid
  // missing a change notification.
  if (is_initialized_)
    SetupWatches();

  const PolicyDefinitionList::Entry* current;
  for (current = policy_list_->begin; current != policy_list_->end; ++current) {
    const string16 name(ASCIIToUTF16(current->name));
    PolicyLevel level = POLICY_LEVEL_MANDATORY;
    PolicyScope scope = POLICY_SCOPE_MACHINE;
    base::Value* value = NULL;

    switch (current->value_type) {
      case base::Value::TYPE_STRING:
        value = ReadChromeStringValue(name, &level, &scope);
        break;

      case base::Value::TYPE_LIST:
        value = ReadChromeStringListValue(name, &level, &scope);
        break;

      case base::Value::TYPE_BOOLEAN:
        value = ReadChromeBooleanValue(name, &level, &scope);
        break;

      case base::Value::TYPE_INTEGER:
        value = ReadChromeIntegerValue(name, &level, &scope);
        break;

      case base::Value::TYPE_DICTIONARY:
        value = ReadChromeDictionaryValue(name, &level, &scope);
        break;

      default:
        NOTREACHED();
    }

    if (value)
      chrome_policies->Set(current->name, level, scope, value);
  }
}

void PolicyLoaderWin::Load3rdPartyPolicies(PolicyBundle* bundle) {
  // Each 3rd party namespace can have policies on both HKLM and HKCU. They
  // should be merged, giving priority to HKLM for policies with the same name.

  // Map of known domain name to their enum values.
  static const struct {
    const char* name;
    PolicyDomain domain;
  } kDomains[] = {
    { "extensions",   POLICY_DOMAIN_EXTENSIONS },
  };

  // Map of policy paths to their corresponding policy level, in decreasing
  // order of priority.
  static const struct {
    const char* path;
    PolicyLevel level;
  } kKeyPaths[] = {
    { "policy",       POLICY_LEVEL_MANDATORY },
    { "recommended",  POLICY_LEVEL_RECOMMENDED },
  };

  // Path where policies for components are stored.
  const string16 kPathPrefix = string16(kRegistryMandatorySubKey) + kPathSep +
                               kThirdParty + kPathSep;

  for (size_t h = 0; h < arraysize(kHives); ++h) {
    HKEY hkey = kHives[h].hive;

    for (size_t d = 0; d < arraysize(kDomains); ++d) {
      // Each subkey under this domain is a component of that domain.
      // |domain_path| == SOFTWARE\Policies\Chromium\3rdparty\<domain>
      string16 domain_path = kPathPrefix + ASCIIToUTF16(kDomains[d].name);

      for (RegistryKeyIterator domain_iterator(hkey, domain_path.c_str());
           domain_iterator.Valid(); ++domain_iterator) {
        string16 component(domain_iterator.Name());
        string16 component_path = domain_path + kPathSep + component;

        // Load the schema for this component's policy, if present.
        scoped_ptr<base::DictionaryValue> schema(
            ReadRegistrySchema(hkey, component_path, kSchema));

        for (size_t k = 0; k < arraysize(kKeyPaths); ++k) {
          string16 path =
              component_path + kPathSep + ASCIIToUTF16(kKeyPaths[k].path);

          scoped_ptr<base::DictionaryValue> dictionary(
              ReadComponentDictionaryValue(hkey, path, schema.get()));
          if (dictionary.get()) {
            PolicyMap policies;
            policies.LoadFrom(
                dictionary.get(), kKeyPaths[k].level, kHives[h].scope);
            // LoadFrom() overwrites any existing values. Use a temporary map
            // and then use MergeFrom(), that only overwrites values with lower
            // priority.
            bundle->Get(kDomains[d].domain, UTF16ToUTF8(component))
                .MergeFrom(policies);
          }
        }
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
