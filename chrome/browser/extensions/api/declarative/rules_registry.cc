// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/declarative/rules_cache_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/extension.h"

namespace {

const char kSuccess[] = "";
const char kDuplicateRuleId[] = "Duplicate rule ID: %s";

scoped_ptr<base::Value> RulesToValue(
    const std::vector<linked_ptr<extensions::RulesRegistry::Rule> >& rules) {
  scoped_ptr<base::ListValue> list(new base::ListValue());
  for (size_t i = 0; i < rules.size(); ++i)
    list->Append(rules[i]->ToValue().release());
  return list.PassAs<base::Value>();
}

std::vector<linked_ptr<extensions::RulesRegistry::Rule> > RulesFromValue(
    const base::Value* value) {
  std::vector<linked_ptr<extensions::RulesRegistry::Rule> > rules;

  const base::ListValue* list = NULL;
  if (!value || !value->GetAsList(&list))
    return rules;

  rules.reserve(list->GetSize());
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* dict = NULL;
    if (!list->GetDictionary(i, &dict))
      continue;
    linked_ptr<extensions::RulesRegistry::Rule> rule(
        new extensions::RulesRegistry::Rule());
    if (extensions::RulesRegistry::Rule::Populate(*dict, rule.get()))
      rules.push_back(rule);
  }

  return rules;
}

std::string ToId(int identifier) {
  return base::StringPrintf("_%d_", identifier);
}

}  // namespace


namespace extensions {

// RulesRegistry

RulesRegistry::RulesRegistry(Profile* profile,
                             const std::string& event_name,
                             content::BrowserThread::ID owner_thread,
                             RulesCacheDelegate* cache_delegate,
                             const WebViewKey& webview_key)
    : profile_(profile),
      owner_thread_(owner_thread),
      event_name_(event_name),
      webview_key_(webview_key),
      ready_(/*signaled=*/!cache_delegate),  // Immediately ready if no cache
                                             // delegate to wait for.
      weak_ptr_factory_(profile ? this : NULL),
      last_generated_rule_identifier_id_(0) {
  if (cache_delegate) {
    cache_delegate_ = cache_delegate->GetWeakPtr();
    cache_delegate->Init(this);
  }
}

std::string RulesRegistry::AddRulesNoFill(
    const std::string& extension_id,
    const std::vector<linked_ptr<Rule> >& rules) {
  DCHECK_CURRENTLY_ON(owner_thread());

  // Verify that all rule IDs are new.
  for (std::vector<linked_ptr<Rule> >::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    const RuleId& rule_id = *((*i)->id);
    // Every rule should have a priority assigned.
    DCHECK((*i)->priority);
    RulesDictionaryKey key(extension_id, rule_id);
    if (rules_.find(key) != rules_.end())
      return base::StringPrintf(kDuplicateRuleId, rule_id.c_str());
  }

  std::string error = AddRulesImpl(extension_id, rules);

  if (!error.empty())
    return error;

  // Commit all rules into |rules_| on success.
  for (std::vector<linked_ptr<Rule> >::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    const RuleId& rule_id = *((*i)->id);
    RulesDictionaryKey key(extension_id, rule_id);
    rules_[key] = *i;
  }

  MaybeProcessChangedRules(extension_id);
  return kSuccess;
}

std::string RulesRegistry::AddRules(
    const std::string& extension_id,
    const std::vector<linked_ptr<Rule> >& rules) {
  DCHECK_CURRENTLY_ON(owner_thread());

  std::string error = CheckAndFillInOptionalRules(extension_id, rules);
  if (!error.empty())
    return error;
  FillInOptionalPriorities(rules);

  return AddRulesNoFill(extension_id, rules);
}

std::string RulesRegistry::RemoveRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  DCHECK_CURRENTLY_ON(owner_thread());

  std::string error = RemoveRulesImpl(extension_id, rule_identifiers);

  if (!error.empty())
    return error;

  for (std::vector<std::string>::const_iterator i = rule_identifiers.begin();
       i != rule_identifiers.end();
       ++i) {
    RulesDictionaryKey lookup_key(extension_id, *i);
    rules_.erase(lookup_key);
  }

  MaybeProcessChangedRules(extension_id);
  RemoveUsedRuleIdentifiers(extension_id, rule_identifiers);
  return kSuccess;
}

std::string RulesRegistry::RemoveAllRules(const std::string& extension_id) {
  std::string result = RulesRegistry::RemoveAllRulesNoStoreUpdate(extension_id);
  MaybeProcessChangedRules(extension_id);  // Now update the prefs and store.
  return result;
}

std::string RulesRegistry::RemoveAllRulesNoStoreUpdate(
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(owner_thread());

  std::string error = RemoveAllRulesImpl(extension_id);

  if (!error.empty())
    return error;

  for (RulesDictionary::const_iterator i = rules_.begin();
      i != rules_.end();) {
    const RulesDictionaryKey& key = i->first;
    ++i;
    if (key.first == extension_id)
      rules_.erase(key);
  }

  RemoveAllUsedRuleIdentifiers(extension_id);
  return kSuccess;
}

void RulesRegistry::GetRules(const std::string& extension_id,
                             const std::vector<std::string>& rule_identifiers,
                             std::vector<linked_ptr<Rule> >* out) {
  DCHECK_CURRENTLY_ON(owner_thread());

  for (std::vector<std::string>::const_iterator i = rule_identifiers.begin();
      i != rule_identifiers.end(); ++i) {
    RulesDictionaryKey lookup_key(extension_id, *i);
    RulesDictionary::iterator entry = rules_.find(lookup_key);
    if (entry != rules_.end())
      out->push_back(entry->second);
  }
}

void RulesRegistry::GetAllRules(const std::string& extension_id,
                                std::vector<linked_ptr<Rule> >* out) {
  DCHECK_CURRENTLY_ON(owner_thread());

  for (RulesDictionary::const_iterator i = rules_.begin();
      i != rules_.end(); ++i) {
    const RulesDictionaryKey& key = i->first;
    if (key.first == extension_id)
      out->push_back(i->second);
  }
}

void RulesRegistry::OnExtensionUnloaded(const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(owner_thread());
  std::string error = RemoveAllRulesImpl(extension_id);
  if (!error.empty())
    LOG(ERROR) << error;
}

void RulesRegistry::OnExtensionUninstalled(const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(owner_thread());
  std::string error = RemoveAllRulesNoStoreUpdate(extension_id);
  if (!error.empty())
    LOG(ERROR) << error;
}

void RulesRegistry::OnExtensionLoaded(const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(owner_thread());
  std::vector<linked_ptr<Rule> > rules;
  GetAllRules(extension_id, &rules);
  std::string error = AddRulesImpl(extension_id, rules);
  if (!error.empty())
    LOG(ERROR) << error;
}

size_t RulesRegistry::GetNumberOfUsedRuleIdentifiersForTesting() const {
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

void RulesRegistry::DeserializeAndAddRules(
    const std::string& extension_id,
    scoped_ptr<base::Value> rules) {
  DCHECK_CURRENTLY_ON(owner_thread());

  AddRulesNoFill(extension_id, RulesFromValue(rules.get()));
}

RulesRegistry::~RulesRegistry() {
}

void RulesRegistry::MarkReady(base::Time storage_init_time) {
  DCHECK_CURRENTLY_ON(owner_thread());

  if (!storage_init_time.is_null()) {
    UMA_HISTOGRAM_TIMES("Extensions.DeclarativeRulesStorageInitialization",
                        base::Time::Now() - storage_init_time);
  }

  ready_.Signal();
}

void RulesRegistry::ProcessChangedRules(const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(owner_thread());

  DCHECK(ContainsKey(process_changed_rules_requested_, extension_id));
  process_changed_rules_requested_[extension_id] = NOT_SCHEDULED_FOR_PROCESSING;

  std::vector<linked_ptr<Rule> > new_rules;
  GetAllRules(extension_id, &new_rules);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RulesCacheDelegate::WriteToStorage,
                 cache_delegate_,
                 extension_id,
                 base::Passed(RulesToValue(new_rules))));
}

void RulesRegistry::MaybeProcessChangedRules(const std::string& extension_id) {
  // Read and initialize |process_changed_rules_requested_[extension_id]| if
  // necessary. (Note that the insertion below will not overwrite
  // |process_changed_rules_requested_[extension_id]| if that already exists.
  std::pair<ProcessStateMap::iterator, bool> insertion =
      process_changed_rules_requested_.insert(std::make_pair(
          extension_id,
          profile_ ? NOT_SCHEDULED_FOR_PROCESSING : NEVER_PROCESS));
  if (insertion.first->second != NOT_SCHEDULED_FOR_PROCESSING)
    return;

  process_changed_rules_requested_[extension_id] = SCHEDULED_FOR_PROCESSING;
  ready_.Post(FROM_HERE,
              base::Bind(&RulesRegistry::ProcessChangedRules,
                         weak_ptr_factory_.GetWeakPtr(),
                         extension_id));
}

bool RulesRegistry::IsUniqueId(const std::string& extension_id,
                               const std::string& rule_id) const {
  RuleIdentifiersMap::const_iterator identifiers =
      used_rule_identifiers_.find(extension_id);
  if (identifiers == used_rule_identifiers_.end())
    return true;
  return identifiers->second.find(rule_id) == identifiers->second.end();
}

std::string RulesRegistry::GenerateUniqueId(const std::string& extension_id) {
  while (!IsUniqueId(extension_id, ToId(last_generated_rule_identifier_id_)))
    ++last_generated_rule_identifier_id_;
  return ToId(last_generated_rule_identifier_id_);
}

std::string RulesRegistry::CheckAndFillInOptionalRules(
    const std::string& extension_id,
    const std::vector<linked_ptr<Rule> >& rules) {
  // IDs we have inserted, in case we need to rollback this operation.
  std::vector<std::string> rollback_log;

  // First we insert all rules with existing identifier, so that generated
  // identifiers cannot collide with identifiers passed by the caller.
  for (std::vector<linked_ptr<Rule> >::const_iterator i = rules.begin();
       i != rules.end();
       ++i) {
    Rule* rule = i->get();
    if (rule->id.get()) {
      std::string id = *(rule->id);
      if (!IsUniqueId(extension_id, id)) {
        RemoveUsedRuleIdentifiers(extension_id, rollback_log);
        return "Id " + id + " was used multiple times.";
      }
      used_rule_identifiers_[extension_id].insert(id);
    }
  }
  // Now we generate IDs in case they were not specified in the rules. This
  // cannot fail so we do not need to keep track of a rollback log.
  for (std::vector<linked_ptr<Rule> >::const_iterator i = rules.begin();
       i != rules.end();
       ++i) {
    Rule* rule = i->get();
    if (!rule->id.get()) {
      rule->id.reset(new std::string(GenerateUniqueId(extension_id)));
      used_rule_identifiers_[extension_id].insert(*(rule->id));
    }
  }
  return std::string();
}

void RulesRegistry::FillInOptionalPriorities(
    const std::vector<linked_ptr<Rule> >& rules) {
  std::vector<linked_ptr<Rule> >::const_iterator i;
  for (i = rules.begin(); i != rules.end(); ++i) {
    if (!(*i)->priority.get())
      (*i)->priority.reset(new int(DEFAULT_PRIORITY));
  }
}

void RulesRegistry::RemoveUsedRuleIdentifiers(
    const std::string& extension_id,
    const std::vector<std::string>& identifiers) {
  std::vector<std::string>::const_iterator i;
  for (i = identifiers.begin(); i != identifiers.end(); ++i)
    used_rule_identifiers_[extension_id].erase(*i);
}

void RulesRegistry::RemoveAllUsedRuleIdentifiers(
    const std::string& extension_id) {
  used_rule_identifiers_.erase(extension_id);
}

}  // namespace extensions
