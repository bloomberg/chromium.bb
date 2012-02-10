// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/test_rules_registry.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative/declarative_api_constants.h"

namespace keys = extensions::declarative_api_constants;

namespace {

const char kSuccess[] = "";
const char kRuleNotFound[] = "Rule not found.";

std::string GetRuleId(DictionaryValue* rule) {
  std::string rule_id;
  CHECK(rule->GetString(keys::kId, &rule_id));
  return rule_id;
}

}  // namespace

namespace extensions {

TestRulesRegistry::TestRulesRegistry() {}

TestRulesRegistry::~TestRulesRegistry() {}

std::string TestRulesRegistry::AddRules(
    const std::string& extension_id,
    const std::vector<DictionaryValue*>& rules) {
  // TODO(battre) this ignores the extension_id but should not.
  for (std::vector<DictionaryValue*>::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    std::string rule_id = GetRuleId(*i);
    // TODO: relax this (check first and abort with returning false).
    CHECK(rules_.find(rule_id) == rules_.end());
    rules_[rule_id] = make_linked_ptr((*i)->DeepCopy());
  }
  return kSuccess;
}

std::string TestRulesRegistry::RemoveRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  // TODO(battre) this ignores the extension_id but should not.
  for (std::vector<std::string>::const_iterator i =
      rule_identifiers.begin(); i != rule_identifiers.end(); ++i) {
    RulesDictionary::iterator entry = rules_.find(*i);
    // TODO: relax this (check first and abort with returning false).
    CHECK(entry != rules_.end());
    rules_.erase(entry);
  }
  return kSuccess;
}

std::string TestRulesRegistry::RemoveAllRules(const std::string& extension_id) {
  // TODO(battre) this ignores the extension_id but should not.
  rules_.clear();
  return kSuccess;
}

std::string TestRulesRegistry::GetRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers,
    std::vector<DictionaryValue*>* out) {
  // TODO(battre) this ignores the extension_id but should not.
  for (std::vector<std::string>::const_iterator i = rule_identifiers.begin();
      i != rule_identifiers.end(); ++i) {
    RulesDictionary::iterator entry = rules_.find(*i);
    if (entry == rules_.end())
      return kRuleNotFound;
    out->push_back(entry->second->DeepCopy());
  }
  return kSuccess;
}

std::string TestRulesRegistry::GetAllRules(
    const std::string& extension_id,
    std::vector<DictionaryValue*>* out) {
  // TODO(battre): this ignores the extension_id.
  for (RulesDictionary::const_iterator i = rules_.begin();
      i != rules_.end(); ++i)
    out->push_back(i->second->DeepCopy());
  return kSuccess;
}

void TestRulesRegistry::OnExtensionUnloaded(const std::string& extension_id) {
  std::vector<std::string> no_rule_identifiers;
  std::string error = RemoveRules(extension_id, no_rule_identifiers);
  if (!error.empty())
    LOG(ERROR) << error;
}

}  // namespace extensions
