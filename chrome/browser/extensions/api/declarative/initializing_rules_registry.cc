// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/initializing_rules_registry.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative/declarative_api_constants.h"

namespace keys = extensions::declarative_api_constants;

namespace {
std::string ToId(int identifier) {
  return StringPrintf("_%d_", identifier);
}
}  // namespace

namespace extensions {

InitializingRulesRegistry::InitializingRulesRegistry(
    scoped_ptr<RulesRegistry> delegate)
    : delegate_(delegate.Pass()),
      last_generated_rule_identifier_id_(0) {
}

InitializingRulesRegistry::~InitializingRulesRegistry() {}

std::string InitializingRulesRegistry::AddRules(
    const std::string& extension_id,
    const std::vector<DictionaryValue*>& rules) {
  std::string error = CheckAndFillInOptionalRules(extension_id, rules);
  if (!error.empty())
    return error;
  FillInOptionalPriorities(rules);
  return delegate_->AddRules(extension_id, rules);
}

std::string InitializingRulesRegistry::RemoveRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  std::string error = delegate_->RemoveRules(extension_id, rule_identifiers);
  if (!error.empty())
    return error;
  RemoveUsedRuleIdentifiers(extension_id, rule_identifiers);
  return "";
}

std::string InitializingRulesRegistry::RemoveAllRules(
    const std::string& extension_id) {
  std::string error = delegate_->RemoveAllRules(extension_id);
  if (!error.empty())
    return error;
  RemoveAllUsedRuleIdentifiers(extension_id);
  return "";
}

std::string InitializingRulesRegistry::GetRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers,
    std::vector<DictionaryValue*>* out) {
  return delegate_->GetRules(extension_id, rule_identifiers, out);
}

std::string InitializingRulesRegistry::GetAllRules(
    const std::string& extension_id,
    std::vector<DictionaryValue*>* out) {
  return delegate_->GetAllRules(extension_id, out);
}

void InitializingRulesRegistry::OnExtensionUnloaded(
    const std::string& extension_id) {
  delegate_->OnExtensionUnloaded(extension_id);
}

bool InitializingRulesRegistry::IsUniqueId(
    const std::string& extension_id,
    const std::string& rule_id) const {
  RuleIdentifiersMap::const_iterator identifiers =
      used_rule_identifiers_.find(extension_id);
  if (identifiers == used_rule_identifiers_.end())
    return true;
  return identifiers->second.find(rule_id) == identifiers->second.end();
}

std::string InitializingRulesRegistry::GenerateUniqueId(
    std::string extension_id) {
  while (!IsUniqueId(extension_id, ToId(last_generated_rule_identifier_id_)))
    ++last_generated_rule_identifier_id_;
  std::string new_id = ToId(last_generated_rule_identifier_id_);
  used_rule_identifiers_[extension_id].insert(new_id);
  return new_id;
}

void InitializingRulesRegistry::RemoveUsedRuleIdentifiers(
    const std::string& extension_id,
    const std::vector<std::string>& identifiers) {
  std::vector<std::string>::const_iterator i;
  for (i = identifiers.begin(); i != identifiers.end(); ++i)
    used_rule_identifiers_[extension_id].erase(*i);
}

void InitializingRulesRegistry::RemoveAllUsedRuleIdentifiers(
    const std::string& extension_id) {
  used_rule_identifiers_.erase(extension_id);
}

std::string InitializingRulesRegistry::CheckAndFillInOptionalRules(
    const std::string& extension_id,
    const std::vector<DictionaryValue*>& rules) {
  // IDs we have inserted, in case we need to rollback this operation.
  std::vector<std::string> rollback_log;

  // First we insert all rules with existing identifier, so that generated
  // identifiers cannot collide with identifiers passed by the caller.
  for (std::vector<DictionaryValue*>::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    DictionaryValue* rule = *i;
    if (rule->HasKey(keys::kId)) {
      std::string id;
      CHECK(rule->GetString(keys::kId, &id));
      if (!IsUniqueId(extension_id, id)) {
        RemoveUsedRuleIdentifiers(extension_id, rollback_log);
        return "Id " + id + " was used multiple times.";
      }
      used_rule_identifiers_[extension_id].insert(id);
    }
  }
  // Now we generate IDs in case they were not specificed in the rules. This
  // cannot fail so we do not need to keep track of a rollback log.
  for (std::vector<DictionaryValue*>::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    DictionaryValue* rule = *i;
    if (!rule->HasKey(keys::kId))
      rule->SetString(keys::kId, GenerateUniqueId(extension_id));
  }
  return "";
}

void InitializingRulesRegistry::FillInOptionalPriorities(
    const std::vector<DictionaryValue*>& rules) {
  std::vector<DictionaryValue*>::const_iterator i;
  for (i = rules.begin(); i != rules.end(); ++i) {
    DictionaryValue* rule = *i;
    if (!rule->HasKey(keys::kPriority))
      rule->SetInteger(keys::kPriority, 100);
  }
}

}  // namespace extensions
