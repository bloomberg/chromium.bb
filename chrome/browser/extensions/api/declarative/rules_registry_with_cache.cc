// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry_with_cache.h"

#include "base/logging.h"
#include "base/stringprintf.h"

namespace {

const char kSuccess[] = "";
const char kDuplicateRuleId[] = "Duplicate rule ID: %s";

}  // namespace

namespace extensions {

RulesRegistryWithCache::RulesRegistryWithCache(Delegate* delegate)
    : delegate_(delegate) {
}

void RulesRegistryWithCache::AddReadyCallback(const base::Closure& callback) {
  ready_callbacks_.push_back(callback);
}

void RulesRegistryWithCache::OnReady() {
  for (size_t i = 0; i < ready_callbacks_.size(); ++i)
    ready_callbacks_[i].Run();
  ready_callbacks_.clear();
}

std::string RulesRegistryWithCache::AddRules(
    const std::string& extension_id,
    const std::vector<linked_ptr<Rule> >& rules) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));

  // Verify that all rule IDs are new.
  for (std::vector<linked_ptr<Rule> >::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    const RuleId& rule_id = *((*i)->id);
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

  NotifyRulesChanged(extension_id);
  return kSuccess;
}

std::string RulesRegistryWithCache::RemoveRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));

  std::string error = RemoveRulesImpl(extension_id, rule_identifiers);

  if (!error.empty())
    return error;

  // Commit removal of rules from |rules_| on success.
  for (std::vector<std::string>::const_iterator i =
      rule_identifiers.begin(); i != rule_identifiers.end(); ++i) {
    RulesDictionaryKey lookup_key(extension_id, *i);
    rules_.erase(lookup_key);
  }

  NotifyRulesChanged(extension_id);
  return kSuccess;
}

std::string RulesRegistryWithCache::RemoveAllRules(
    const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));

  std::string error = RemoveAllRulesImpl(extension_id);

  if (!error.empty())
    return error;

  // Commit removal of rules from |rules_| on success.
  for (RulesDictionary::const_iterator i = rules_.begin();
      i != rules_.end();) {
    const RulesDictionaryKey& key = i->first;
    ++i;
    if (key.first == extension_id)
      rules_.erase(key);
  }

  NotifyRulesChanged(extension_id);
  return kSuccess;
}

std::string RulesRegistryWithCache::GetRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers,
    std::vector<linked_ptr<RulesRegistry::Rule> >* out) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));

  for (std::vector<std::string>::const_iterator i = rule_identifiers.begin();
      i != rule_identifiers.end(); ++i) {
    RulesDictionaryKey lookup_key(extension_id, *i);
    RulesDictionary::iterator entry = rules_.find(lookup_key);
    if (entry != rules_.end())
      out->push_back(entry->second);
  }
  return kSuccess;
}

std::string RulesRegistryWithCache::GetAllRules(
    const std::string& extension_id,
    std::vector<linked_ptr<RulesRegistry::Rule> >* out) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));

  for (RulesDictionary::const_iterator i = rules_.begin();
      i != rules_.end(); ++i) {
    const RulesDictionaryKey& key = i->first;
    if (key.first == extension_id)
      out->push_back(i->second);
  }
  return kSuccess;
}

void RulesRegistryWithCache::OnExtensionUnloaded(
    const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));
  std::string error = RemoveAllRules(extension_id);
  if (!error.empty())
    LOG(ERROR) << error;
}

void RulesRegistryWithCache::NotifyRulesChanged(
    const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));

  if (delegate_.get())
    delegate_->OnRulesChanged(this, extension_id);
}

RulesRegistryWithCache::~RulesRegistryWithCache() {}

}  // namespace extensions
