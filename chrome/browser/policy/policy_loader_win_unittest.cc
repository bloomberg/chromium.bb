// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_loader_win.h"

#include <userenv.h>
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <iterator>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/async_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_provider_test.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/preg_parser_win.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_schema/json_schema_constants.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace schema = json_schema_constants;

using base::win::RegKey;

namespace policy {

namespace {

// Constants for registry key names.
const wchar_t kPathSep[] = L"\\";
const wchar_t kThirdParty[] = L"3rdparty";
const wchar_t kMandatory[] = L"policy";
const wchar_t kRecommended[] = L"recommended";
const wchar_t kSchema[] = L"schema";

// Installs |value| in the given registry |path| and |hive|, under the key
// |name|. Returns false on errors.
// Some of the possible Value types are stored after a conversion (e.g. doubles
// are stored as strings), and can only be retrieved if a corresponding schema
// is written.
bool InstallValue(const base::Value& value,
                  HKEY hive,
                  const string16& path,
                  const string16& name) {
  // KEY_ALL_ACCESS causes the ctor to create the key if it does not exist yet.
  RegKey key(hive, path.c_str(), KEY_ALL_ACCESS);
  EXPECT_TRUE(key.Valid());
  switch (value.GetType()) {
    case base::Value::TYPE_NULL:
      return key.WriteValue(name.c_str(), L"") == ERROR_SUCCESS;

    case base::Value::TYPE_BOOLEAN: {
      bool bool_value;
      if (!value.GetAsBoolean(&bool_value))
        return false;
      return key.WriteValue(name.c_str(), bool_value ? 1 : 0) == ERROR_SUCCESS;
    }

    case base::Value::TYPE_INTEGER: {
      int int_value;
      if (!value.GetAsInteger(&int_value))
        return false;
      return key.WriteValue(name.c_str(), int_value) == ERROR_SUCCESS;
    }

    case base::Value::TYPE_DOUBLE: {
      double double_value;
      if (!value.GetAsDouble(&double_value))
        return false;
      string16 str_value = UTF8ToUTF16(base::DoubleToString(double_value));
      return key.WriteValue(name.c_str(), str_value.c_str()) == ERROR_SUCCESS;
    }

    case base::Value::TYPE_STRING: {
      string16 str_value;
      if (!value.GetAsString(&str_value))
        return false;
      return key.WriteValue(name.c_str(), str_value.c_str()) == ERROR_SUCCESS;
    }

    case base::Value::TYPE_DICTIONARY: {
      const base::DictionaryValue* sub_dict = NULL;
      if (!value.GetAsDictionary(&sub_dict))
        return false;
      for (base::DictionaryValue::Iterator it(*sub_dict);
           !it.IsAtEnd(); it.Advance()) {
        if (!InstallValue(it.value(), hive, path + kPathSep + name,
                          UTF8ToUTF16(it.key()))) {
          return false;
        }
      }
      return true;
    }

    case base::Value::TYPE_LIST: {
      const base::ListValue* list = NULL;
      if (!value.GetAsList(&list))
        return false;
      for (size_t i = 0; i < list->GetSize(); ++i) {
        const base::Value* item;
        if (!list->Get(i, &item))
          return false;
        if (!InstallValue(*item, hive, path + kPathSep + name,
                          base::UintToString16(i + 1))) {
          return false;
        }
      }
      return true;
    }

    case base::Value::TYPE_BINARY:
      return false;
  }
  NOTREACHED();
  return false;
}

// Builds a JSON schema that represents the types contained in |value|.
// Ownership is transferred to the caller.
base::DictionaryValue* BuildSchema(const base::Value& value) {
  base::DictionaryValue* schema = new base::DictionaryValue();
  switch (value.GetType()) {
    case base::Value::TYPE_NULL:
      schema->SetString(schema::kType, "null");
      break;
    case base::Value::TYPE_BOOLEAN:
      schema->SetString(schema::kType, "boolean");
      break;
    case base::Value::TYPE_INTEGER:
      schema->SetString(schema::kType, "integer");
      break;
    case base::Value::TYPE_DOUBLE:
      schema->SetString(schema::kType, "number");
      break;
    case base::Value::TYPE_STRING:
      schema->SetString(schema::kType, "string");
      break;

    case base::Value::TYPE_LIST: {
      // Assumes every list element has the same type.
      const base::ListValue* list = NULL;
      if (value.GetAsList(&list) && !list->empty()) {
        schema->SetString(schema::kType, "array");
        schema->Set(schema::kItems, BuildSchema(**list->begin()));
      }
      break;
    }

    case base::Value::TYPE_DICTIONARY: {
      const base::DictionaryValue* dict = NULL;
      if (value.GetAsDictionary(&dict)) {
        base::DictionaryValue* properties = new base::DictionaryValue();
        for (base::DictionaryValue::Iterator it(*dict);
             !it.IsAtEnd(); it.Advance()) {
          properties->Set(it.key(), BuildSchema(it.value()));
        }
        schema->SetString(schema::kType, "object");
        schema->Set(schema::kProperties, properties);
      }
      break;
    }

    case base::Value::TYPE_BINARY:
      break;
  }
  return schema;
}

// Writes a JSON |schema| at the registry entry |name| at |path|
// in the given |hive|. Returns false on failure.
bool WriteSchema(const base::DictionaryValue& schema,
                 HKEY hive,
                 const string16& path,
                 const string16& name) {
  std::string encoded;
  base::JSONWriter::Write(&schema, &encoded);
  if (encoded.empty())
    return false;
  string16 encoded16 = UTF8ToUTF16(encoded);
  // KEY_ALL_ACCESS causes the ctor to create the key if it does not exist yet.
  RegKey key(hive, path.c_str(), KEY_ALL_ACCESS);
  EXPECT_TRUE(key.Valid());
  return key.WriteValue(name.c_str(), encoded16.c_str()) == ERROR_SUCCESS;
}

// Builds a JSON schema for |value| and writes it at the registry entry |name|
// at |path| in the given |hive|. Returns false on failure.
bool InstallSchema(const base::Value& value,
                   HKEY hive,
                   const string16& path,
                   const string16& name) {
  scoped_ptr<base::DictionaryValue> schema_dict(BuildSchema(value));
  return WriteSchema(*schema_dict, hive, path, name);
}

// This class provides sandboxing and mocking for the parts of the Windows
// Registry implementing Group Policy. It prepares two temporary sandbox keys,
// one for HKLM and one for HKCU. A test's calls to the registry are redirected
// by Windows to these sandboxes, allowing the tests to manipulate and access
// policy as if it were active, but without actually changing the parts of the
// Registry that are managed by Group Policy.
class ScopedGroupPolicyRegistrySandbox {
 public:
  ScopedGroupPolicyRegistrySandbox();
  ~ScopedGroupPolicyRegistrySandbox();

 private:
  void ActivateOverrides();
  void RemoveOverrides();

  // Deletes the sandbox keys.
  void DeleteKeys();

  std::wstring key_name_;

  // Keys are created for the lifetime of a test to contain
  // the sandboxed HKCU and HKLM hives, respectively.
  RegKey temp_hkcu_hive_key_;
  RegKey temp_hklm_hive_key_;

  DISALLOW_COPY_AND_ASSIGN(ScopedGroupPolicyRegistrySandbox);
};

// A test harness that feeds policy via the Chrome GPO registry subtree.
class RegistryTestHarness : public PolicyProviderTestHarness,
                            public AppliedGPOListProvider {
 public:
  RegistryTestHarness(HKEY hive, PolicyScope scope);
  virtual ~RegistryTestHarness();

  // PolicyProviderTestHarness:
  virtual void SetUp() OVERRIDE;

  virtual ConfigurationPolicyProvider* CreateProvider(
      const PolicyDefinitionList* policy_list) OVERRIDE;

  virtual void InstallEmptyPolicy() OVERRIDE;
  virtual void InstallStringPolicy(const std::string& policy_name,
                                   const std::string& policy_value) OVERRIDE;
  virtual void InstallIntegerPolicy(const std::string& policy_name,
                                    int policy_value) OVERRIDE;
  virtual void InstallBooleanPolicy(const std::string& policy_name,
                                    bool policy_value) OVERRIDE;
  virtual void InstallStringListPolicy(
      const std::string& policy_name,
      const base::ListValue* policy_value) OVERRIDE;
  virtual void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::DictionaryValue* policy_value) OVERRIDE;
  virtual void Install3rdPartyPolicy(
      const base::DictionaryValue* policies) OVERRIDE;

  // AppliedGPOListProvider:
  virtual DWORD GetAppliedGPOList(DWORD flags,
                                  LPCTSTR machine_name,
                                  PSID sid_user,
                                  GUID* extension_guid,
                                  PGROUP_POLICY_OBJECT* gpo_list) OVERRIDE;
  virtual BOOL FreeGPOList(PGROUP_POLICY_OBJECT gpo_list) OVERRIDE;

  // Creates a harness instance that will install policy in HKCU or HKLM,
  // respectively.
  static PolicyProviderTestHarness* CreateHKCU();
  static PolicyProviderTestHarness* CreateHKLM();

 private:
  HKEY hive_;

  ScopedGroupPolicyRegistrySandbox registry_sandbox_;

  DISALLOW_COPY_AND_ASSIGN(RegistryTestHarness);
};

// A test harness that generates PReg files for the provider to read.
class PRegTestHarness : public PolicyProviderTestHarness,
                        public AppliedGPOListProvider {
 public:
  PRegTestHarness();
  virtual ~PRegTestHarness();

  // PolicyProviderTestHarness:
  virtual void SetUp() OVERRIDE;

  virtual ConfigurationPolicyProvider* CreateProvider(
      const PolicyDefinitionList* policy_list) OVERRIDE;

  virtual void InstallEmptyPolicy() OVERRIDE;
  virtual void InstallStringPolicy(const std::string& policy_name,
                                   const std::string& policy_value) OVERRIDE;
  virtual void InstallIntegerPolicy(const std::string& policy_name,
                                    int policy_value) OVERRIDE;
  virtual void InstallBooleanPolicy(const std::string& policy_name,
                                    bool policy_value) OVERRIDE;
  virtual void InstallStringListPolicy(
      const std::string& policy_name,
      const base::ListValue* policy_value) OVERRIDE;
  virtual void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::DictionaryValue* policy_value) OVERRIDE;
  virtual void Install3rdPartyPolicy(
      const base::DictionaryValue* policies) OVERRIDE;

  // AppliedGPOListProvider:
  virtual DWORD GetAppliedGPOList(DWORD flags,
                                  LPCTSTR machine_name,
                                  PSID sid_user,
                                  GUID* extension_guid,
                                  PGROUP_POLICY_OBJECT* gpo_list) OVERRIDE;
  virtual BOOL FreeGPOList(PGROUP_POLICY_OBJECT gpo_list) OVERRIDE;

  // Creates a harness instance.
  static PolicyProviderTestHarness* Create();

 private:
  // Helper to append a string16 to an uint8 buffer.
  static void AppendChars(std::vector<uint8>* buffer, const string16& chars);

  // Appends a record with the given fields to the PReg file.
  void AppendRecordToPRegFile(const string16& path,
                              const std::string& key,
                              DWORD type,
                              DWORD size,
                              uint8* data);

  // Appends the given DWORD |value| for |path| + |key| to the PReg file.
  void AppendDWORDToPRegFile(const string16& path,
                             const std::string& key,
                             DWORD value);

  // Appends the given string |value| for |path| + |key| to the PReg file.
  void AppendStringToPRegFile(const string16& path,
                              const std::string& key,
                              const std::string& value);

  // Appends the given policy |value| for |path| + |key| to the PReg file,
  // converting and recursing as necessary.
  void AppendPolicyToPRegFile(const string16& path,
                              const std::string& key,
                              const base::Value* value);

  base::ScopedTempDir temp_dir_;
  base::FilePath preg_file_path_;
  GROUP_POLICY_OBJECT gpo_;

  DISALLOW_COPY_AND_ASSIGN(PRegTestHarness);
};

ScopedGroupPolicyRegistrySandbox::ScopedGroupPolicyRegistrySandbox() {
  // Generate a unique registry key for the override for each test. This
  // makes sure that tests executing in parallel won't delete each other's
  // key, at DeleteKeys().
  key_name_ = ASCIIToWide(base::StringPrintf(
        "SOFTWARE\\chromium unittest %d",
        base::Process::Current().pid()));
  std::wstring hklm_key_name = key_name_ + L"\\HKLM";
  std::wstring hkcu_key_name = key_name_ + L"\\HKCU";

  // Create the subkeys to hold the overridden HKLM and HKCU
  // policy settings.
  temp_hklm_hive_key_.Create(HKEY_CURRENT_USER,
                             hklm_key_name.c_str(),
                             KEY_ALL_ACCESS);
  temp_hkcu_hive_key_.Create(HKEY_CURRENT_USER,
                             hkcu_key_name.c_str(),
                             KEY_ALL_ACCESS);

  ActivateOverrides();
}

ScopedGroupPolicyRegistrySandbox::~ScopedGroupPolicyRegistrySandbox() {
  RemoveOverrides();
  DeleteKeys();
}

void ScopedGroupPolicyRegistrySandbox::ActivateOverrides() {
  ASSERT_HRESULT_SUCCEEDED(RegOverridePredefKey(HKEY_LOCAL_MACHINE,
                                                temp_hklm_hive_key_.Handle()));
  ASSERT_HRESULT_SUCCEEDED(RegOverridePredefKey(HKEY_CURRENT_USER,
                                                temp_hkcu_hive_key_.Handle()));
}

void ScopedGroupPolicyRegistrySandbox::RemoveOverrides() {
  ASSERT_HRESULT_SUCCEEDED(RegOverridePredefKey(HKEY_LOCAL_MACHINE, 0));
  ASSERT_HRESULT_SUCCEEDED(RegOverridePredefKey(HKEY_CURRENT_USER, 0));
}

void ScopedGroupPolicyRegistrySandbox::DeleteKeys() {
  RegKey key(HKEY_CURRENT_USER, key_name_.c_str(), KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  key.DeleteKey(L"");
}

RegistryTestHarness::RegistryTestHarness(HKEY hive, PolicyScope scope)
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY, scope), hive_(hive) {}

RegistryTestHarness::~RegistryTestHarness() {}

void RegistryTestHarness::SetUp() {}

ConfigurationPolicyProvider* RegistryTestHarness::CreateProvider(
    const PolicyDefinitionList* policy_list) {
  scoped_ptr<AsyncPolicyLoader> loader(
      new PolicyLoaderWin(policy_list, kRegistryChromePolicyKey, this));
  return new AsyncPolicyProvider(loader.Pass());
}

void RegistryTestHarness::InstallEmptyPolicy() {}

void RegistryTestHarness::InstallStringPolicy(
    const std::string& policy_name,
    const std::string& policy_value) {
  RegKey key(hive_, kRegistryChromePolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  ASSERT_HRESULT_SUCCEEDED(key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                                          UTF8ToUTF16(policy_value).c_str()));
}

void RegistryTestHarness::InstallIntegerPolicy(
    const std::string& policy_name,
    int policy_value) {
  RegKey key(hive_, kRegistryChromePolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 static_cast<DWORD>(policy_value));
}

void RegistryTestHarness::InstallBooleanPolicy(
    const std::string& policy_name,
    bool policy_value) {
  RegKey key(hive_, kRegistryChromePolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 static_cast<DWORD>(policy_value));
}

void RegistryTestHarness::InstallStringListPolicy(
    const std::string& policy_name,
    const base::ListValue* policy_value) {
  RegKey key(hive_,
             (string16(kRegistryChromePolicyKey) + ASCIIToUTF16("\\") +
              UTF8ToUTF16(policy_name)).c_str(),
             KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  int index = 1;
  for (base::ListValue::const_iterator element(policy_value->begin());
       element != policy_value->end();
       ++element) {
    std::string element_value;
    if (!(*element)->GetAsString(&element_value))
      continue;
    std::string name(base::IntToString(index++));
    key.WriteValue(UTF8ToUTF16(name).c_str(),
                   UTF8ToUTF16(element_value).c_str());
  }
}

void RegistryTestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  std::string json;
  base::JSONWriter::Write(policy_value, &json);
  RegKey key(hive_, kRegistryChromePolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 UTF8ToUTF16(json).c_str());
}

void RegistryTestHarness::Install3rdPartyPolicy(
    const base::DictionaryValue* policies) {
  // The first level entries are domains, and the second level entries map
  // components to their policy.
  const string16 kPathPrefix = string16(kRegistryChromePolicyKey) + kPathSep +
                               kThirdParty + kPathSep;
  for (base::DictionaryValue::Iterator domain(*policies);
       !domain.IsAtEnd(); domain.Advance()) {
    const base::DictionaryValue* components = NULL;
    if (!domain.value().GetAsDictionary(&components)) {
      ADD_FAILURE();
      continue;
    }
    for (base::DictionaryValue::Iterator component(*components);
         !component.IsAtEnd(); component.Advance()) {
      const string16 path = kPathPrefix +
                            UTF8ToUTF16(domain.key()) + kPathSep +
                            UTF8ToUTF16(component.key());
      InstallValue(component.value(), hive_, path, kMandatory);
      EXPECT_TRUE(InstallSchema(component.value(), hive_, path, kSchema));
    }
  }
}

DWORD RegistryTestHarness::GetAppliedGPOList(DWORD flags,
                                             LPCTSTR machine_name,
                                             PSID sid_user,
                                             GUID* extension_guid,
                                             PGROUP_POLICY_OBJECT* gpo_list) {
  *gpo_list = NULL;
  return ERROR_ACCESS_DENIED;
}

BOOL RegistryTestHarness::FreeGPOList(PGROUP_POLICY_OBJECT gpo_list) {
  return TRUE;
}

// static
PolicyProviderTestHarness* RegistryTestHarness::CreateHKCU() {
  return new RegistryTestHarness(HKEY_CURRENT_USER, POLICY_SCOPE_USER);
}

// static
PolicyProviderTestHarness* RegistryTestHarness::CreateHKLM() {
  return new RegistryTestHarness(HKEY_LOCAL_MACHINE, POLICY_SCOPE_MACHINE);
}

PRegTestHarness::PRegTestHarness()
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE) {}

PRegTestHarness::~PRegTestHarness() {}

void PRegTestHarness::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  preg_file_path_ = temp_dir_.path().Append(PolicyLoaderWin::kPRegFileName);
  ASSERT_TRUE(file_util::WriteFile(preg_file_path_,
                                   preg_parser::kPRegFileHeader,
                                   arraysize(preg_parser::kPRegFileHeader)));

  memset(&gpo_, 0, sizeof(GROUP_POLICY_OBJECT));
  gpo_.lpFileSysPath = const_cast<wchar_t*>(temp_dir_.path().value().c_str());
}

ConfigurationPolicyProvider* PRegTestHarness::CreateProvider(
    const PolicyDefinitionList* policy_list) {
  scoped_ptr<AsyncPolicyLoader> loader(
      new PolicyLoaderWin(policy_list, kRegistryChromePolicyKey, this));
  return new AsyncPolicyProvider(loader.Pass());
}

void PRegTestHarness::InstallEmptyPolicy() {}

void PRegTestHarness::InstallStringPolicy(const std::string& policy_name,
                                          const std::string& policy_value) {
  AppendStringToPRegFile(kRegistryChromePolicyKey, policy_name, policy_value);
}

void PRegTestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                           int policy_value) {
  AppendDWORDToPRegFile(kRegistryChromePolicyKey, policy_name, policy_value);
}

void PRegTestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                           bool policy_value) {
  AppendDWORDToPRegFile(kRegistryChromePolicyKey, policy_name, policy_value);
}

void PRegTestHarness::InstallStringListPolicy(
    const std::string& policy_name,
    const base::ListValue* policy_value) {
  AppendPolicyToPRegFile(kRegistryChromePolicyKey, policy_name, policy_value);
}

void PRegTestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  std::string json;
  base::JSONWriter::Write(policy_value, &json);
  AppendStringToPRegFile(kRegistryChromePolicyKey, policy_name, json);
}

void PRegTestHarness::Install3rdPartyPolicy(
    const base::DictionaryValue* policies) {
  // The first level entries are domains, and the second level entries map
  // components to their policy.
  const string16 kPathPrefix = string16(kRegistryChromePolicyKey) + kPathSep +
                               kThirdParty + kPathSep;
  for (base::DictionaryValue::Iterator domain(*policies);
       !domain.IsAtEnd(); domain.Advance()) {
    const base::DictionaryValue* components = NULL;
    if (!domain.value().GetAsDictionary(&components)) {
      ADD_FAILURE();
      continue;
    }
    const string16 domain_path = kPathPrefix + UTF8ToUTF16(domain.key());
    for (base::DictionaryValue::Iterator component(*components);
         !component.IsAtEnd(); component.Advance()) {
      const string16 component_path =
          domain_path + kPathSep + UTF8ToUTF16(component.key());
      AppendPolicyToPRegFile(component_path, UTF16ToUTF8(kMandatory),
                             &component.value());

      scoped_ptr<base::DictionaryValue> schema_dict(
          BuildSchema(component.value()));
      std::string schema_json;
      base::JSONWriter::Write(schema_dict.get(), &schema_json);
      if (!schema_json.empty()) {
        AppendStringToPRegFile(component_path, UTF16ToUTF8(kSchema),
                               schema_json);
      }
    }
  }
}

DWORD PRegTestHarness::GetAppliedGPOList(DWORD flags,
                                         LPCTSTR machine_name,
                                         PSID sid_user,
                                         GUID* extension_guid,
                                         PGROUP_POLICY_OBJECT* gpo_list) {
  *gpo_list = flags & GPO_LIST_FLAG_MACHINE ? &gpo_ : NULL;
  return ERROR_SUCCESS;
}

BOOL PRegTestHarness::FreeGPOList(PGROUP_POLICY_OBJECT gpo_list) {
  return TRUE;
}

// static
PolicyProviderTestHarness* PRegTestHarness::Create() {
  return new PRegTestHarness();
}

// static
void PRegTestHarness::AppendChars(std::vector<uint8>* buffer,
                                  const string16& chars) {
  for (string16::const_iterator c(chars.begin()); c != chars.end(); ++c) {
    buffer->push_back(*c & 0xff);
    buffer->push_back((*c >> 8) & 0xff);
  }
}

void PRegTestHarness::AppendRecordToPRegFile(const string16& path,
                                             const std::string& key,
                                             DWORD type,
                                             DWORD size,
                                             uint8* data) {
  std::vector<uint8> buffer;
  AppendChars(&buffer, L"[");
  AppendChars(&buffer, path);
  AppendChars(&buffer, string16(L"\0;", 2));
  AppendChars(&buffer, UTF8ToUTF16(key));
  AppendChars(&buffer, string16(L"\0;", 2));
  type = base::ByteSwapToLE32(type);
  uint8* type_data = reinterpret_cast<uint8*>(&type);
  buffer.insert(buffer.end(), type_data, type_data + sizeof(DWORD));
  AppendChars(&buffer, L";");
  size = base::ByteSwapToLE32(size);
  uint8* size_data = reinterpret_cast<uint8*>(&size);
  buffer.insert(buffer.end(), size_data, size_data + sizeof(DWORD));
  AppendChars(&buffer, L";");
  buffer.insert(buffer.end(), data, data + size);
  AppendChars(&buffer, L"]");

  ASSERT_EQ(buffer.size(),
            file_util::AppendToFile(
                preg_file_path_,
                reinterpret_cast<const char*>(vector_as_array(&buffer)),
                buffer.size()));
}

void PRegTestHarness::AppendDWORDToPRegFile(const string16& path,
                                            const std::string& key,
                                            DWORD value) {
  value = base::ByteSwapToLE32(value);
  AppendRecordToPRegFile(path, key, REG_DWORD, sizeof(DWORD),
                         reinterpret_cast<uint8*>(&value));
}

void PRegTestHarness::AppendStringToPRegFile(const string16& path,
                                             const std::string& key,
                                             const std::string& value) {
  string16 string16_value(UTF8ToUTF16(value));
  std::vector<char16> data;
  std::transform(string16_value.begin(), string16_value.end(),
                 std::back_inserter(data), std::ptr_fun(base::ByteSwapToLE16));
  data.push_back(base::ByteSwapToLE16(L'\0'));

  AppendRecordToPRegFile(path, key, REG_SZ, data.size() * sizeof(char16),
                         reinterpret_cast<uint8*>(vector_as_array(&data)));
}

void PRegTestHarness::AppendPolicyToPRegFile(const string16& path,
                                             const std::string& key,
                                             const base::Value* value) {
  switch (value->GetType()) {
    case base::Value::TYPE_BOOLEAN: {
      bool boolean_value = false;
      ASSERT_TRUE(value->GetAsBoolean(&boolean_value));
      AppendDWORDToPRegFile(path, key, boolean_value);
      break;
    }
    case base::Value::TYPE_INTEGER: {
      int int_value = 0;
      ASSERT_TRUE(value->GetAsInteger(&int_value));
      AppendDWORDToPRegFile(path, key, int_value);
      break;
    }
    case base::Value::TYPE_DOUBLE: {
      double double_value = 0;
      std::string string_value;
      ASSERT_TRUE(value->GetAsDouble(&double_value));
      AppendStringToPRegFile(path, key, base::DoubleToString(double_value));
      break;
    }
    case base::Value::TYPE_STRING: {
      std::string string_value;
      ASSERT_TRUE(value->GetAsString(&string_value));
      AppendStringToPRegFile(path, key, string_value);
      break;
    }
    case base::Value::TYPE_DICTIONARY: {
      string16 subpath = path + kPathSep + UTF8ToUTF16(key);
      const base::DictionaryValue* dict = NULL;
      ASSERT_TRUE(value->GetAsDictionary(&dict));
      for (base::DictionaryValue::Iterator entry(*dict); !entry.IsAtEnd();
           entry.Advance()) {
        AppendPolicyToPRegFile(subpath, entry.key(), &entry.value());
      }
      break;
    }
    case base::Value::TYPE_LIST: {
      string16 subpath = path + kPathSep + UTF8ToUTF16(key);
      const base::ListValue* list = NULL;
      ASSERT_TRUE(value->GetAsList(&list));
      for (size_t i = 0; i < list->GetSize(); ++i) {
        const base::Value* entry = NULL;
        ASSERT_TRUE(list->Get(i, &entry));
        AppendPolicyToPRegFile(subpath, base::IntToString(i + 1), entry);
      }
      break;
    }
    case base::Value::TYPE_BINARY:
    case base::Value::TYPE_NULL: {
      ADD_FAILURE();
      break;
    }
  }
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_CASE_P(
    PolicyProviderWinTest,
    ConfigurationPolicyProviderTest,
    testing::Values(RegistryTestHarness::CreateHKCU,
                    RegistryTestHarness::CreateHKLM,
                    PRegTestHarness::Create));

// Instantiate abstract test case for 3rd party policy reading tests.
INSTANTIATE_TEST_CASE_P(
    ThirdPartyPolicyProviderWinTest,
    Configuration3rdPartyPolicyProviderTest,
    testing::Values(RegistryTestHarness::CreateHKCU,
                    RegistryTestHarness::CreateHKLM,
                    PRegTestHarness::Create));

// Test cases for windows policy provider specific functionality.
class PolicyLoaderWinTest : public PolicyTestBase,
                            public AppliedGPOListProvider {
 protected:
  // The policy key this tests places data under. This must match the data
  // files in chrome/test/data/policy/gpo.
  static const char16 kTestPolicyKey[];

  PolicyLoaderWinTest()
      : gpo_list_(NULL),
        gpo_list_status_(ERROR_ACCESS_DENIED) {}
  virtual ~PolicyLoaderWinTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
    test_data_dir_ = test_data_dir_.AppendASCII("policy").AppendASCII("gpo");
  }

  // AppliedGPOListProvider:
  virtual DWORD GetAppliedGPOList(DWORD flags,
                                  LPCTSTR machine_name,
                                  PSID sid_user,
                                  GUID* extension_guid,
                                  PGROUP_POLICY_OBJECT* gpo_list) OVERRIDE {
    *gpo_list = gpo_list_;
    return gpo_list_status_;
  }
  virtual BOOL FreeGPOList(PGROUP_POLICY_OBJECT gpo_list) OVERRIDE {
    return TRUE;
  }

  void InitGPO(GROUP_POLICY_OBJECT* gpo,
               DWORD options,
               const base::FilePath& path,
               GROUP_POLICY_OBJECT* next,
               GROUP_POLICY_OBJECT* prev) {
    memset(gpo, 0, sizeof(GROUP_POLICY_OBJECT));
    gpo->dwOptions = options;
    gpo->lpFileSysPath = const_cast<wchar_t*>(path.value().c_str());
    gpo->pNext = next;
    gpo->pPrev = prev;
  }

  bool Matches(const PolicyBundle& expected) {
    PolicyLoaderWin loader(&test_policy_definitions::kList, kTestPolicyKey,
                           this);
    scoped_ptr<PolicyBundle> loaded(loader.Load());
    return loaded->Equals(expected);
  }

  void InstallRegistrySentinel() {
    RegKey hklm_key(HKEY_CURRENT_USER, kTestPolicyKey, KEY_ALL_ACCESS);
    ASSERT_TRUE(hklm_key.Valid());
    hklm_key.WriteValue(
        UTF8ToUTF16(test_policy_definitions::kKeyString).c_str(),
        UTF8ToUTF16("registry").c_str());
  }

  bool MatchesRegistrySentinel() {
    base::DictionaryValue expected_policy;
    expected_policy.SetString(test_policy_definitions::kKeyString, "registry");
    PolicyBundle expected;
    expected.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .LoadFrom(&expected_policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
    return Matches(expected);
  }

  bool MatchesTestBundle() {
    base::DictionaryValue expected_policy;
    expected_policy.SetBoolean(test_policy_definitions::kKeyBoolean, true);
    expected_policy.SetString(test_policy_definitions::kKeyString, "GPO");
    expected_policy.SetInteger(test_policy_definitions::kKeyInteger, 42);
    scoped_ptr<base::ListValue> list(new base::ListValue());
    list->AppendString("GPO 1");
    list->AppendString("GPO 2");
    expected_policy.Set(test_policy_definitions::kKeyStringList,
                        list.release());
    PolicyBundle expected;
    expected.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .LoadFrom(&expected_policy, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE);
    return Matches(expected);
  }

  ScopedGroupPolicyRegistrySandbox registry_sandbox_;
  PGROUP_POLICY_OBJECT gpo_list_;
  DWORD gpo_list_status_;
  base::FilePath test_data_dir_;
};

const char16 PolicyLoaderWinTest::kTestPolicyKey[] =
    L"SOFTWARE\\Policies\\Chromium";

TEST_F(PolicyLoaderWinTest, HKLMOverHKCU) {
  RegKey hklm_key(HKEY_LOCAL_MACHINE, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(hklm_key.Valid());
  hklm_key.WriteValue(UTF8ToUTF16(test_policy_definitions::kKeyString).c_str(),
                      UTF8ToUTF16("hklm").c_str());
  RegKey hkcu_key(HKEY_CURRENT_USER, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(hkcu_key.Valid());
  hkcu_key.WriteValue(UTF8ToUTF16(test_policy_definitions::kKeyString).c_str(),
                      UTF8ToUTF16("hkcu").c_str());

  PolicyBundle expected;
  expected.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(test_policy_definitions::kKeyString,
           POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_MACHINE,
           base::Value::CreateStringValue("hklm"));
  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, Load3rdPartyWithoutSchema) {
  base::DictionaryValue dict;
  dict.SetString("str", "string value");
  dict.SetInteger("int", 123);
  dict.Set("subdict", dict.DeepCopy());
  dict.Set("subsubdict", dict.DeepCopy());
  dict.Set("subsubsubdict", dict.DeepCopy());

  base::DictionaryValue policy_dict;
  policy_dict.Set("extensions.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.policy",
                  dict.DeepCopy());
  policy_dict.Set("extensions.bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.policy",
                  dict.DeepCopy());
  EXPECT_TRUE(InstallValue(policy_dict, HKEY_LOCAL_MACHINE,
                           kTestPolicyKey, kThirdParty));

  PolicyBundle expected;
  expected.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                               "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))
      .LoadFrom(&dict, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE);
  expected.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                               "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"))
      .LoadFrom(&dict, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE);
  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, Merge3rdPartyPolicies) {
  // Policy for the same extension will be provided at the 4 level/scope
  // combinations, to verify that they overlap as expected.

  const string16 kPathSuffix =
      kTestPolicyKey + ASCIIToUTF16("\\3rdparty\\extensions\\merge");

  const char kUserMandatory[] = "user-mandatory";
  const char kUserRecommended[] = "user-recommended";
  const char kMachineMandatory[] = "machine-mandatory";
  const char kMachineRecommended[] = "machine-recommended";

  base::DictionaryValue policy;
  policy.SetString("a", kMachineMandatory);
  EXPECT_TRUE(InstallValue(policy, HKEY_LOCAL_MACHINE,
                           kPathSuffix, kMandatory));
  policy.SetString("a", kUserMandatory);
  policy.SetString("b", kUserMandatory);
  EXPECT_TRUE(InstallValue(policy, HKEY_CURRENT_USER,
                           kPathSuffix, kMandatory));
  policy.SetString("a", kMachineRecommended);
  policy.SetString("b", kMachineRecommended);
  policy.SetString("c", kMachineRecommended);
  EXPECT_TRUE(InstallValue(policy, HKEY_LOCAL_MACHINE,
                           kPathSuffix, kRecommended));
  policy.SetString("a", kUserRecommended);
  policy.SetString("b", kUserRecommended);
  policy.SetString("c", kUserRecommended);
  policy.SetString("d", kUserRecommended);
  EXPECT_TRUE(InstallValue(policy, HKEY_CURRENT_USER,
                           kPathSuffix, kRecommended));

  PolicyBundle expected;
  PolicyMap& expected_policy =
      expected.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "merge"));
  expected_policy.Set("a", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      base::Value::CreateStringValue(kMachineMandatory));
  expected_policy.Set("b", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      base::Value::CreateStringValue(kUserMandatory));
  expected_policy.Set("c", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                      base::Value::CreateStringValue(kMachineRecommended));
  expected_policy.Set("d", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                      base::Value::CreateStringValue(kUserRecommended));
  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, LoadStringEncodedValues) {
  // Create a dictionary with all the types that can be stored encoded in a
  // string, to pass to InstallSchema(). Also build an equivalent dictionary
  // with the encoded values, to pass to InstallValue().
  base::DictionaryValue policy;
  policy.Set("null", base::Value::CreateNullValue());
  policy.SetBoolean("bool", true);
  policy.SetInteger("int", -123);
  policy.SetDouble("double", 456.78e9);
  base::ListValue list;
  list.Append(policy.DeepCopy());
  list.Append(policy.DeepCopy());
  policy.Set("list", list.DeepCopy());
  // Encode |policy| before adding the "dict" entry.
  std::string encoded_dict;
  base::JSONWriter::Write(&policy, &encoded_dict);
  ASSERT_FALSE(encoded_dict.empty());
  policy.Set("dict", policy.DeepCopy());

  std::string encoded_list;
  base::JSONWriter::Write(&list, &encoded_list);
  ASSERT_FALSE(encoded_list.empty());
  base::DictionaryValue encoded_policy;
  encoded_policy.SetString("null", "");
  encoded_policy.SetString("bool", "1");
  encoded_policy.SetString("int", "-123");
  encoded_policy.SetString("double", "456.78e9");
  encoded_policy.SetString("list", encoded_list);
  encoded_policy.SetString("dict", encoded_dict);

  const string16 kPathSuffix =
      kTestPolicyKey + ASCIIToUTF16("\\3rdparty\\extensions\\string");
  EXPECT_TRUE(InstallSchema(policy, HKEY_CURRENT_USER, kPathSuffix, kSchema));
  EXPECT_TRUE(
      InstallValue(encoded_policy, HKEY_CURRENT_USER, kPathSuffix, kMandatory));

  PolicyBundle expected;
  expected.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "string"))
      .LoadFrom(&policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, LoadIntegerEncodedValues) {
  base::DictionaryValue policy;
  policy.SetBoolean("bool", true);
  policy.SetInteger("int", 123);
  policy.SetDouble("double", 456.0);

  base::DictionaryValue encoded_policy;
  encoded_policy.SetInteger("bool", 1);
  encoded_policy.SetInteger("int", 123);
  encoded_policy.SetInteger("double", 456);

  const string16 kPathSuffix =
      kTestPolicyKey + ASCIIToUTF16("\\3rdparty\\extensions\\int");
  EXPECT_TRUE(InstallSchema(policy, HKEY_CURRENT_USER, kPathSuffix, kSchema));
  EXPECT_TRUE(
      InstallValue(encoded_policy, HKEY_CURRENT_USER, kPathSuffix, kMandatory));

  PolicyBundle expected;
  expected.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "int"))
      .LoadFrom(&policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, DefaultPropertySchemaType) {
  // Build a schema for an "object" with a default schema for its properties.
  base::DictionaryValue default_schema;
  default_schema.SetString(schema::kType, "number");
  base::DictionaryValue integer_schema;
  integer_schema.SetString(schema::kType, "integer");
  base::DictionaryValue properties;
  properties.Set("special-int1", integer_schema.DeepCopy());
  properties.Set("special-int2", integer_schema.DeepCopy());
  base::DictionaryValue schema;
  schema.SetString(schema::kType, "object");
  schema.Set(schema::kProperties, properties.DeepCopy());
  schema.Set(schema::kAdditionalProperties, default_schema.DeepCopy());

  const string16 kPathSuffix =
      kTestPolicyKey + ASCIIToUTF16("\\3rdparty\\extensions\\test");
  EXPECT_TRUE(WriteSchema(schema, HKEY_CURRENT_USER, kPathSuffix, kSchema));

  // Write some test values.
  base::DictionaryValue policy;
  // These special values have a specific schema for them.
  policy.SetInteger("special-int1", 123);
  policy.SetString("special-int2", "-456");
  // Other values default to be loaded as doubles.
  policy.SetInteger("double1", 789.0);
  policy.SetString("double2", "123.456e7");
  policy.SetString("invalid", "omg");
  EXPECT_TRUE(InstallValue(policy, HKEY_CURRENT_USER, kPathSuffix, kMandatory));

  base::DictionaryValue expected_policy;
  expected_policy.SetInteger("special-int1", 123);
  expected_policy.SetInteger("special-int2", -456);
  expected_policy.SetDouble("double1", 789.0);
  expected_policy.SetDouble("double2", 123.456e7);
  expected_policy.Set("invalid", base::Value::CreateNullValue());
  PolicyBundle expected;
  expected.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "test"))
      .LoadFrom(&expected_policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, AppliedPolicyNotPresent) {
  InstallRegistrySentinel();
  gpo_list_ = NULL;
  gpo_list_status_ = ERROR_SUCCESS;

  PolicyBundle empty;
  EXPECT_TRUE(Matches(empty));
}

TEST_F(PolicyLoaderWinTest, AppliedPolicyEmpty) {
  InstallRegistrySentinel();
  base::FilePath gpo_dir(test_data_dir_.AppendASCII("empty"));
  GROUP_POLICY_OBJECT gpo;
  InitGPO(&gpo, 0, gpo_dir, NULL, NULL);
  gpo_list_ = &gpo;
  gpo_list_status_ = ERROR_SUCCESS;

  PolicyBundle empty;
  EXPECT_TRUE(Matches(empty));
}

TEST_F(PolicyLoaderWinTest, AppliedPolicyNonExistingFile) {
  InstallRegistrySentinel();
  GROUP_POLICY_OBJECT gpo;
  InitGPO(&gpo, 0, test_data_dir_, NULL, NULL);
  gpo_list_ = &gpo;
  gpo_list_status_ = ERROR_SUCCESS;

  EXPECT_TRUE(MatchesRegistrySentinel());
}

TEST_F(PolicyLoaderWinTest, AppliedPolicyBadPath) {
  InstallRegistrySentinel();
  base::FilePath gpo_dir(test_data_dir_.AppendASCII("bad"));
  GROUP_POLICY_OBJECT gpo;
  InitGPO(&gpo, 0, gpo_dir, NULL, NULL);
  gpo_list_ = &gpo;
  gpo_list_status_ = ERROR_SUCCESS;

  EXPECT_TRUE(MatchesRegistrySentinel());
}

TEST_F(PolicyLoaderWinTest, AppliedPolicyPresent) {
  InstallRegistrySentinel();
  base::FilePath gpo_dir(test_data_dir_.AppendASCII("test1"));
  GROUP_POLICY_OBJECT gpo;
  InitGPO(&gpo, 0, gpo_dir, NULL, NULL);
  gpo_list_ = &gpo;
  gpo_list_status_ = ERROR_SUCCESS;

  EXPECT_TRUE(MatchesTestBundle());
}

TEST_F(PolicyLoaderWinTest, AppliedPolicyMerged) {
  InstallRegistrySentinel();
  base::FilePath gpo1_dir(test_data_dir_.AppendASCII("test2"));
  base::FilePath gpo2_dir(test_data_dir_.AppendASCII("test1"));
  GROUP_POLICY_OBJECT gpo1;
  GROUP_POLICY_OBJECT gpo2;
  InitGPO(&gpo1, 0, gpo1_dir, &gpo2, NULL);
  InitGPO(&gpo2, 0, gpo2_dir, NULL, &gpo1);
  gpo_list_ = &gpo1;
  gpo_list_status_ = ERROR_SUCCESS;

  EXPECT_TRUE(MatchesTestBundle());
}

TEST_F(PolicyLoaderWinTest, AppliedPolicyDisabled) {
  InstallRegistrySentinel();
  base::FilePath gpo1_dir(test_data_dir_.AppendASCII("test1"));
  base::FilePath gpo2_dir(test_data_dir_.AppendASCII("test2"));
  GROUP_POLICY_OBJECT gpo1;
  GROUP_POLICY_OBJECT gpo2;
  InitGPO(&gpo1, 0, gpo1_dir, &gpo2, NULL);
  InitGPO(&gpo2, GPO_FLAG_DISABLE, gpo2_dir, NULL, &gpo1);
  gpo_list_ = &gpo1;
  gpo_list_status_ = ERROR_SUCCESS;

  EXPECT_TRUE(MatchesTestBundle());
}

TEST_F(PolicyLoaderWinTest, AppliedPolicyForcedPolicy) {
  InstallRegistrySentinel();
  base::FilePath gpo1_dir(test_data_dir_.AppendASCII("test1"));
  base::FilePath gpo2_dir(test_data_dir_.AppendASCII("test2"));
  GROUP_POLICY_OBJECT gpo1;
  GROUP_POLICY_OBJECT gpo2;
  InitGPO(&gpo1, GPO_FLAG_FORCE, gpo1_dir, &gpo2, NULL);
  InitGPO(&gpo2, 0, gpo2_dir, NULL, &gpo1);
  gpo_list_ = &gpo1;
  gpo_list_status_ = ERROR_SUCCESS;

  EXPECT_TRUE(MatchesTestBundle());
}

TEST_F(PolicyLoaderWinTest, AppliedPolicyUNCPath) {
  InstallRegistrySentinel();
  base::FilePath gpo_dir(test_data_dir_.AppendASCII("test1"));
  base::FilePath unc_path(L"\\\\some_share\\GPO");
  GROUP_POLICY_OBJECT gpo1;
  GROUP_POLICY_OBJECT gpo2;
  InitGPO(&gpo1, 0, gpo_dir, &gpo2, NULL);
  InitGPO(&gpo2, 0, unc_path, NULL, &gpo1);
  gpo_list_ = &gpo1;
  gpo_list_status_ = ERROR_SUCCESS;

  EXPECT_TRUE(MatchesRegistrySentinel());
}

TEST_F(PolicyLoaderWinTest, LoadExtensionPolicyAlternativeSpelling) {
  base::FilePath gpo_dir(
      test_data_dir_.AppendASCII("extension_alternative_spelling"));
  GROUP_POLICY_OBJECT gpo;
  InitGPO(&gpo, 0, gpo_dir, NULL, NULL);
  gpo_list_ = &gpo;
  gpo_list_status_ = ERROR_SUCCESS;

  PolicyBundle expected;
  base::DictionaryValue expected_a;
  expected_a.SetInteger("policy 1", 1);
  expected_a.SetInteger("policy 2", 3);
  expected.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                               "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))
      .LoadFrom(&expected_a, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE);
  base::DictionaryValue expected_b;
  expected_b.SetInteger("policy 1", 2);
  expected.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                               "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"))
      .LoadFrom(&expected_b, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE);
  EXPECT_TRUE(Matches(expected));
}

}  // namespace policy
