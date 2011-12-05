// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/automation_id.h"

#include "base/stringprintf.h"
#include "base/values.h"

using base::DictionaryValue;
using base::Value;

// static
bool AutomationId::FromValue(
    Value* value, AutomationId* id, std::string* error) {
  DictionaryValue* dict;
  if (!value->GetAsDictionary(&dict)) {
    *error = "automation ID must be a dictionary";
    return false;
  }
  int type;
  if (!dict->GetInteger("type", &type)) {
    *error = "automation ID 'type' missing or invalid";
    return false;
  }
  std::string type_id;
  if (!dict->GetString("id", &type_id)) {
    *error = "automation ID 'type_id' missing or invalid";
    return false;
  }
  *id = AutomationId(static_cast<Type>(type), type_id);
  return true;
}

// static
bool AutomationId::FromValueInDictionary(
    DictionaryValue* dict,
    const std::string& key,
    AutomationId* id,
    std::string* error) {
  Value* id_value;
  if (!dict->Get(key, &id_value)) {
    *error = base::StringPrintf("automation ID '%s' missing", key.c_str());
    return false;
  }
  return FromValue(id_value, id, error);
}

AutomationId::AutomationId() : type_(kTypeInvalid) { }

AutomationId::AutomationId(Type type, const std::string& id)
    : type_(type), id_(id) { }

bool AutomationId::operator==(const AutomationId& id) const {
  return type_ == id.type_ && id_ == id.id_;
}

DictionaryValue* AutomationId::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("type", type_);
  dict->SetString("id", id_);
  return dict;
}

bool AutomationId::is_valid() const {
  return type_ != kTypeInvalid;
}

AutomationId::Type AutomationId::type() const {
  return type_;
}

const std::string& AutomationId::id() const {
  return id_;
}
