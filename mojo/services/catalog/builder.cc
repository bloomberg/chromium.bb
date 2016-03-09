// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/builder.h"

#include "base/values.h"
#include "mojo/services/catalog/store.h"
#include "mojo/shell/public/cpp/names.h"

namespace catalog {

CapabilityFilter BuildCapabilityFilter(const base::DictionaryValue& value) {
  CapabilityFilter filter;
  base::DictionaryValue::Iterator it(value);
  for (; !it.IsAtEnd(); it.Advance()) {
    const base::ListValue* values = nullptr;
    CHECK(it.value().GetAsList(&values));
    AllowedInterfaces interfaces;
    for (auto i = values->begin(); i != values->end(); ++i) {
      std::string iface_name;
      const base::Value* v = *i;
      CHECK(v->GetAsString(&iface_name));
      interfaces.insert(iface_name);
    }
    filter[it.key()] = interfaces;
  }
  return filter;
}

Entry BuildEntry(const base::DictionaryValue& value) {
  Entry entry;
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
  entry.capabilities = BuildCapabilityFilter(*capabilities);

  return entry;
}

void SerializeEntry(const Entry& entry, base::DictionaryValue** value) {
  *value = new base::DictionaryValue;
  (*value)->SetString(Store::kNameKey, entry.name);
  (*value)->SetString(Store::kDisplayNameKey, entry.display_name);
  base::DictionaryValue* capabilities = new base::DictionaryValue;
  for (const auto& pair : entry.capabilities) {
    scoped_ptr<base::ListValue> interfaces(new base::ListValue);
    for (const auto& iface_name : pair.second)
      interfaces->AppendString(iface_name);
    capabilities->Set(pair.first, std::move(interfaces));
  }
  (*value)->Set(Store::kCapabilitiesKey, make_scoped_ptr(capabilities));
}

}  // namespace catalog
