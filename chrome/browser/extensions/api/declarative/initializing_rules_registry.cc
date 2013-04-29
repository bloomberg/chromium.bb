// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/initializing_rules_registry.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"

namespace {
std::string ToId(int identifier) {
  return base::StringPrintf("_%d_", identifier);
}
}  // namespace

namespace extensions {

InitializingRulesRegistry::InitializingRulesRegistry(
    scoped_refptr<RulesRegistry> delegate)
    : RulesRegistry(delegate->owner_thread(), delegate->event_name()),
      delegate_(delegate),
      last_generated_rule_identifier_id_(0) {}

std::string InitializingRulesRegistry::AddRules(
    const std::string& extension_id,
    const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) {
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
  return std::string();
}

std::string InitializingRulesRegistry::RemoveAllRules(
    const std::string& extension_id) {
  std::string error = delegate_->RemoveAllRules(extension_id);
  if (!error.empty())
    return error;
  RemoveAllUsedRuleIdentifiers(extension_id);
  return std::string();
}

std::string InitializingRulesRegistry::GetRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers,
    std::vector<linked_ptr<RulesRegistry::Rule> >* out) {
  return delegate_->GetRules(extension_id, rule_identifiers, out);
}

std::string InitializingRulesRegistry::GetAllRules(
    const std::string& extension_id,
    std::vector<linked_ptr<RulesRegistry::Rule> >* out) {
  return delegate_->GetAllRules(extension_id, out);
}

void InitializingRulesRegistry::OnExtensionUnloaded(
    const std::string& extension_id) {
  delegate_->OnExtensionUnloaded(extension_id);
  used_rule_identifiers_.erase(extension_id);
}

size_t
InitializingRulesRegistry::GetNumberOfUsedRuleIdentifiersForTesting() const {
  size_t entry_count = 0u;
  for (RuleIdentifiersMap::const_iterator extension =
           used_rule_identifiers_.begin();
       extension != used_rule_identifiers_.end();
       ++extension) {
    // Each extension is counted as 1 just for being there. Otherwise we miss
    // keys with empty values.
    entry_count += 1u + extension->second.size();
  }
  return entry_count;
}

InitializingRulesRegistry::~InitializingRulesRegistry() {}

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
    const std::string& extension_id) {
  while (!IsUniqueId(extension_id, ToId(last_generated_rule_identifier_id_)))
    ++last_generated_rule_identifier_id_;
  return ToId(last_generated_rule_identifier_id_);
}

std::string InitializingRulesRegistry::CheckAndFillInOptionalRules(
    const std::string& extension_id,
    const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) {
  // IDs we have inserted, in case we need to rollback this operation.
  std::vector<std::string> rollback_log;

  // First we insert all rules with existing identifier, so that generated
  // identifiers cannot collide with identifiers passed by the caller.
  for (std::vector<linked_ptr<RulesRegistry::Rule> >::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    RulesRegistry::Rule* rule = i->get();
    if (rule->id.get()) {
      std::string id = *(rule->id);
      if (!IsUniqueId(extension_id, id)) {
        RemoveUsedRuleIdentifiers(extension_id, rollback_log);
        return "Id " + id + " was used multiple times.";
      }
      used_rule_identifiers_[extension_id].insert(id);
    }
  }
  // Now we generate IDs in case they were not specificed in the rules. This
  // cannot fail so we do not need to keep track of a rollback log.
  for (std::vector<linked_ptr<RulesRegistry::Rule> >::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    RulesRegistry::Rule* rule = i->get();
    if (!rule->id.get()) {
      rule->id.reset(new std::string(GenerateUniqueId(extension_id)));
      used_rule_identifiers_[extension_id].insert(*(rule->id));
    }
  }
  return std::string();
}

void InitializingRulesRegistry::FillInOptionalPriorities(
    const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) {
  std::vector<linked_ptr<RulesRegistry::Rule> >::const_iterator i;
  for (i = rules.begin(); i != rules.end(); ++i) {
    if (!(*i)->priority.get())
      (*i)->priority.reset(new int(DEFAULT_PRIORITY));
  }
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

}  // namespace extensions
