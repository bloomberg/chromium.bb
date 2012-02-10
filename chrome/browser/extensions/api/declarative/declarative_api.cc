// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/declarative_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/declarative/declarative_api_constants.h"
#include "chrome/browser/extensions/api/declarative/rules_registry.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"

namespace keys = extensions::declarative_api_constants;

namespace {

// Adds all entries from |list| to |out|. Assumes that all entries of |list|
// are strings. Returns true if successful.
bool AddAllStringValues(ListValue* list, std::vector<std::string>* out) {
  for (ListValue::iterator i = list->begin(); i != list->end(); ++i) {
    std::string value;
    if (!(*i)->GetAsString(&value))
      return false;
    out->push_back(value);
  }
  return true;
}

}  // namespace

namespace extensions {

bool AddRulesFunction::RunImpl() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));

  ListValue* rules_list = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(1, &rules_list));

  std::vector<DictionaryValue*> rules;
  for (ListValue::iterator i = rules_list->begin();
       i != rules_list->end();
       ++i) {
    DictionaryValue* rule = NULL;
    EXTENSION_FUNCTION_VALIDATE((*i)->GetAsDictionary(&rule));
    rules.push_back(rule);
  }

  RulesRegistryService* rules_registry_service =
      profile()->GetExtensionService()->GetRulesRegistryService();
  RulesRegistry* rules_registry =
      rules_registry_service->GetRulesRegistry(event_name);
  if (!rules_registry) {
    error_ = keys::kInvalidEventName;
    return false;
  }

  error_ = rules_registry->AddRules(extension_id(), rules);
  if (!error_.empty())
    return false;

  result_.reset(rules_list->DeepCopy());
  return true;
}

bool RemoveRulesFunction::RunImpl() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));

  Value* rule_identifiers = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(1, &rule_identifiers));

  RulesRegistryService* rules_registry_service =
      profile()->GetExtensionService()->GetRulesRegistryService();
  RulesRegistry* rules_registry =
      rules_registry_service->GetRulesRegistry(event_name);
  if (!rules_registry) {
    error_ = keys::kInvalidEventName;
    return false;
  }

  switch (rule_identifiers->GetType()) {
    case Value::TYPE_NULL:
      error_ = rules_registry->RemoveAllRules(extension_id());
      break;
    case Value::TYPE_LIST: {
      std::vector<std::string> rule_identifiers_list;
      EXTENSION_FUNCTION_VALIDATE(
          AddAllStringValues(static_cast<ListValue*>(rule_identifiers),
                             &rule_identifiers_list));
      error_ = rules_registry->RemoveRules(extension_id(),
                                           rule_identifiers_list);
      break;
    }
    default:
      error_ = keys::kInvalidDatatype;
      break;
  }
  return error_.empty();
}

bool GetRulesFunction::RunImpl() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));

  Value* rule_identifiers = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(1, &rule_identifiers));

  RulesRegistryService* rules_registry_service =
      profile()->GetExtensionService()->GetRulesRegistryService();
  RulesRegistry* rules_registry =
      rules_registry_service->GetRulesRegistry(event_name);
  if (!rules_registry) {
    error_ = keys::kInvalidEventName;
    return false;
  }

  std::vector<DictionaryValue*> rules;
  switch (rule_identifiers->GetType()) {
    case Value::TYPE_NULL:
      error_ = rules_registry->GetAllRules(extension_id(), &rules);
      break;
    case Value::TYPE_LIST: {
      std::vector<std::string> rule_identifiers_list;
      EXTENSION_FUNCTION_VALIDATE(
          AddAllStringValues(static_cast<ListValue*>(rule_identifiers),
                             &rule_identifiers_list));
      error_ = rules_registry->GetRules(extension_id(), rule_identifiers_list,
                                        &rules);
      break;
    }
    default:
      error_ = keys::kInvalidDatatype;
      break;
  }

  if (!error_.empty())
    return false;

  scoped_ptr<ListValue> result(new ListValue);
  for (std::vector<DictionaryValue*>::iterator i = rules.begin();
      i != rules.end(); ++i)
    result->Append(*i);
  result_.reset(result.release());

  return true;
}

}  // namespace extensions
