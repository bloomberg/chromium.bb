// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/test_rules_registry.h"

#include "base/logging.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kSuccess[] = "";
const char kRuleNotFound[] = "Rule not found.";

}  // namespace

namespace extensions {

TestRulesRegistry::TestRulesRegistry()
    : owner_thread_(content::BrowserThread::UI) {}

TestRulesRegistry::~TestRulesRegistry() {}

std::string TestRulesRegistry::AddRules(
    const std::string& extension_id,
    const std::vector<linked_ptr<Rule> >& rules) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));
  // TODO(battre) this ignores the extension_id but should not.
  for (std::vector<linked_ptr<Rule> >::const_iterator i =
      rules.begin(); i != rules.end(); ++i) {
    std::string rule_id = *((*i)->id);
    // TODO: relax this (check first and abort with returning false).
    CHECK(rules_.find(rule_id) == rules_.end());
    rules_[rule_id] = *i;
  }
  return kSuccess;
}

std::string TestRulesRegistry::RemoveRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));
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
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));
  // TODO(battre) this ignores the extension_id but should not.
  rules_.clear();
  return kSuccess;
}

std::string TestRulesRegistry::GetRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers,
    std::vector<linked_ptr<RulesRegistry::Rule> >* out) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));
  // TODO(battre) this ignores the extension_id but should not.
  for (std::vector<std::string>::const_iterator i = rule_identifiers.begin();
      i != rule_identifiers.end(); ++i) {
    RulesDictionary::iterator entry = rules_.find(*i);
    if (entry == rules_.end())
      return kRuleNotFound;
    out->push_back(entry->second);
  }
  return kSuccess;
}

std::string TestRulesRegistry::GetAllRules(
    const std::string& extension_id,
    std::vector<linked_ptr<RulesRegistry::Rule> >* out) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));
  // TODO(battre): this ignores the extension_id.
  for (RulesDictionary::const_iterator i = rules_.begin();
      i != rules_.end(); ++i)
    out->push_back(i->second);
  return kSuccess;
}

void TestRulesRegistry::OnExtensionUnloaded(const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(GetOwnerThread()));
  std::string error = RemoveAllRules(extension_id);
  if (!error.empty())
    LOG(ERROR) << error;
}

content::BrowserThread::ID TestRulesRegistry::GetOwnerThread() const {
  return owner_thread_;
}

}  // namespace extensions
