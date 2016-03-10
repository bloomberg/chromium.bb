// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/builder.h"

#include "base/values.h"
#include "mojo/services/catalog/store.h"
#include "mojo/shell/public/cpp/capabilities.h"
#include "mojo/shell/public/cpp/names.h"

// TODO(beng): this code should do better error handling instead of CHECKing so
// much.

namespace catalog {

mojo::CapabilitySpec BuildCapabilitiesV0(
    const base::DictionaryValue& value) {
  mojo::CapabilitySpec capabilities;
  base::DictionaryValue::Iterator it(value);
  for (; !it.IsAtEnd(); it.Advance()) {
    const base::ListValue* values = nullptr;
    CHECK(it.value().GetAsList(&values));
    mojo::CapabilityRequest spec;
    for (auto i = values->begin(); i != values->end(); ++i) {
      mojo::Interface interface_name;
      const base::Value* v = *i;
      CHECK(v->GetAsString(&interface_name));
      spec.interfaces.insert(interface_name);
    }
    capabilities.required[it.key()] = spec;
  }
  return capabilities;
}

void ReadStringSet(const base::ListValue& list_value,
                   std::set<std::string>* string_set) {
  DCHECK(string_set);
  for (auto i = list_value.begin(); i != list_value.end(); ++i) {
    std::string value;
    const base::Value* value_value = *i;
    CHECK(value_value->GetAsString(&value));
    string_set->insert(value);
  }
}

void ReadStringSetFromValue(const base::Value& value,
                            std::set<std::string>* string_set) {
  const base::ListValue* list_value = nullptr;
  CHECK(value.GetAsList(&list_value));
  ReadStringSet(*list_value, string_set);
}

void ReadStringSetFromDictionary(const base::DictionaryValue& dictionary,
                                 const std::string& key,
                                 std::set<std::string>* string_set) {
  const base::ListValue* list_value = nullptr;
  if (dictionary.HasKey(key))
    CHECK(dictionary.GetList(key, &list_value));
  if (list_value)
    ReadStringSet(*list_value, string_set);
}

mojo::CapabilitySpec BuildCapabilitiesV1(
    const base::DictionaryValue& value) {
  mojo::CapabilitySpec capabilities;

  const base::DictionaryValue* provided_value = nullptr;
  if (value.HasKey(Store::kCapabilities_ProvidedKey)) {
    CHECK(value.GetDictionary(Store::kCapabilities_ProvidedKey,
                              &provided_value));
  }
  if (provided_value) {
    mojo::CapabilityRequest provided;
    base::DictionaryValue::Iterator it(*provided_value);
    for(; !it.IsAtEnd(); it.Advance()) {
      mojo::Interfaces interfaces;
      ReadStringSetFromValue(it.value(), &interfaces);
      capabilities.provided[it.key()] = interfaces;
    }
  }

  const base::DictionaryValue* required_value = nullptr;
  if (value.HasKey(Store::kCapabilities_RequiredKey)) {
    CHECK(value.GetDictionary(Store::kCapabilities_RequiredKey,
                              &required_value));
  }
  if (required_value) {
    base::DictionaryValue::Iterator it(*required_value);
    for (; !it.IsAtEnd(); it.Advance()) {
      mojo::CapabilityRequest spec;
      const base::DictionaryValue* entry_value = nullptr;
      CHECK(it.value().GetAsDictionary(&entry_value));
      ReadStringSetFromDictionary(
          *entry_value, Store::kCapabilities_ClassesKey, &spec.classes);
      ReadStringSetFromDictionary(
          *entry_value, Store::kCapabilities_InterfacesKey, &spec.interfaces);
      capabilities.required[it.key()] = spec;
    }
  }
  return capabilities;
}

Entry BuildEntry(const base::DictionaryValue& value) {
  Entry entry;
  int manifest_version = 0;
  if (value.HasKey(Store::kManifestVersionKey))
    CHECK(value.GetInteger(Store::kManifestVersionKey, &manifest_version));
  std::string name_string;
  CHECK(value.GetString(Store::kNameKey, &name_string));
  CHECK(mojo::IsValidName(name_string)) << "Invalid Name: " << name_string;
  entry.name = name_string;
  if (value.HasKey(Store::kQualifierKey)) {
    CHECK(value.GetString(Store::kQualifierKey, &entry.qualifier));
  } else {
    entry.qualifier = mojo::GetNamePath(name_string);
  }
  CHECK(value.GetString(Store::kDisplayNameKey, &entry.display_name));
  const base::DictionaryValue* capabilities = nullptr;
  CHECK(value.GetDictionary(Store::kCapabilitiesKey, &capabilities));
  if (manifest_version == 0)
    entry.capabilities = BuildCapabilitiesV0(*capabilities);
  else
    entry.capabilities = BuildCapabilitiesV1(*capabilities);
  return entry;
}

scoped_ptr<base::DictionaryValue> SerializeEntry(const Entry& entry) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetInteger(Store::kManifestVersionKey, 1);
  value->SetString(Store::kNameKey, entry.name);
  value->SetString(Store::kDisplayNameKey, entry.display_name);
  value->SetString(Store::kQualifierKey, entry.qualifier);
  scoped_ptr<base::DictionaryValue> spec(new base::DictionaryValue);

  scoped_ptr<base::DictionaryValue> provided(new base::DictionaryValue);
  for (const auto& i : entry.capabilities.provided) {
    scoped_ptr<base::ListValue> interfaces(new base::ListValue);
    for (const auto& interface_name : i.second)
      interfaces->AppendString(interface_name);
    provided->Set(i.first, std::move(interfaces));
  }
  spec->Set(Store::kCapabilities_ProvidedKey, std::move(provided));

  scoped_ptr<base::DictionaryValue> required(new base::DictionaryValue);
  for (const auto& i : entry.capabilities.required) {
    scoped_ptr<base::DictionaryValue> request(new base::DictionaryValue);
    scoped_ptr<base::ListValue> classes(new base::ListValue);
    for (const auto& class_name : i.second.classes)
      classes->AppendString(class_name);
    request->Set(Store::kCapabilities_ClassesKey, std::move(classes));
    scoped_ptr<base::ListValue> interfaces(new base::ListValue);
    for (const auto& interface_name : i.second.interfaces)
      interfaces->AppendString(interface_name);
    request->Set(Store::kCapabilities_InterfacesKey, std::move(interfaces));
    required->Set(i.first, std::move(request));
  }
  spec->Set(Store::kCapabilities_RequiredKey, std::move(required));

  value->Set(Store::kCapabilitiesKey, std::move(spec));
  return value;
}

}  // namespace catalog
