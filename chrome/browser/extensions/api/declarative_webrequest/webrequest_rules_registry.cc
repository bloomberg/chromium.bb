// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rules_registry.h"

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "chrome/browser/extensions/extension_system.h"
#include "net/url_request/url_request.h"

namespace extensions {

WebRequestRulesRegistry::WebRequestRulesRegistry(Profile* profile) {
  if (profile)
    extension_info_map_ = ExtensionSystem::Get(profile)->info_map();
}

std::set<WebRequestRule::GlobalRuleId>
WebRequestRulesRegistry::GetMatches(net::URLRequest* request,
                                    RequestStages request_stage) {
  std::set<WebRequestRule::GlobalRuleId> result;

  // Figure out for which rules the URL match conditions were fulfilled.
  typedef std::set<URLMatcherConditionSet::ID> URLMatches;
  URLMatches url_matches = url_matcher_.MatchURL(request->url());

  // Then we need to check for each of these, whether the other
  // WebRequestConditionAttributes are also fulfilled.
  for (URLMatches::iterator url_match = url_matches.begin();
       url_match != url_matches.end(); ++url_match) {
    RuleTriggers::iterator rule_trigger = rule_triggers_.find(*url_match);
    CHECK(rule_trigger != rule_triggers_.end());

    WebRequestRule* rule = rule_trigger->second;
    if (rule->conditions().IsFulfilled(*url_match, request, request_stage))
      result.insert(rule->id());
  }

  return result;
}

std::list<LinkedPtrEventResponseDelta> WebRequestRulesRegistry::CreateDeltas(
    net::URLRequest* request,
    RequestStages request_stage) {
  std::set<WebRequestRule::GlobalRuleId> matches =
      GetMatches(request, request_stage);

  std::list<LinkedPtrEventResponseDelta> result;

  for (std::set<WebRequestRule::GlobalRuleId>::iterator i = matches.begin();
       i != matches.end(); ++i) {
    RulesMap::const_iterator rule = webrequest_rules_.find(*i);
    CHECK(rule != webrequest_rules_.end());
    std::list<LinkedPtrEventResponseDelta> rule_result =
        rule->second->CreateDeltas(request, request_stage);
    result.splice(result.begin(), rule_result);
  }

  return result;
}

std::string WebRequestRulesRegistry::AddRulesImpl(
    const std::string& extension_id,
    const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) {
  base::Time extension_installation_time =
      GetExtensionInstallationTime(extension_id);

  std::string error;
  RulesMap new_webrequest_rules;

  for (std::vector<linked_ptr<RulesRegistry::Rule> >::const_iterator rule =
       rules.begin(); rule != rules.end(); ++rule) {
    WebRequestRule::GlobalRuleId rule_id(extension_id, *(*rule)->id);
    DCHECK(webrequest_rules_.find(rule_id) == webrequest_rules_.end());

    scoped_ptr<WebRequestRule> webrequest_rule(
        WebRequestRule::Create(url_matcher_.condition_factory(), extension_id,
                               extension_installation_time, *rule, &error));
    if (!error.empty()) {
      // We don't return here, because we want to clear temporary
      // condition sets in the url_matcher_.
      break;
    }

    new_webrequest_rules[rule_id] = make_linked_ptr(webrequest_rule.release());
  }

  if (!error.empty()) {
    // Clean up temporary condition sets created during rule creation.
    url_matcher_.ClearUnusedConditionSets();
    return error;
  }

  // Wohoo, everything worked fine.
  webrequest_rules_.insert(new_webrequest_rules.begin(),
                           new_webrequest_rules.end());

  // Create the triggers.
  for (RulesMap::iterator i = new_webrequest_rules.begin();
       i != new_webrequest_rules.end(); ++i) {
    URLMatcherConditionSet::Vector url_condition_sets;
    const WebRequestConditionSet& conditions = i->second->conditions();
    conditions.GetURLMatcherConditionSets(&url_condition_sets);
    for (URLMatcherConditionSet::Vector::iterator j =
         url_condition_sets.begin(); j != url_condition_sets.end(); ++j) {
      rule_triggers_[(*j)->id()] = i->second.get();
    }
  }

  // Register url patterns in url_matcher_.
  URLMatcherConditionSet::Vector all_new_condition_sets;
  for (RulesMap::iterator i = new_webrequest_rules.begin();
       i != new_webrequest_rules.end(); ++i) {
    i->second->conditions().GetURLMatcherConditionSets(&all_new_condition_sets);
  }
  url_matcher_.AddConditionSets(all_new_condition_sets);

  return "";
}

std::string WebRequestRulesRegistry::RemoveRulesImpl(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  // URLMatcherConditionSet IDs that can be removed from URLMatcher.
  std::vector<URLMatcherConditionSet::ID> remove_from_url_matcher;

  for (std::vector<std::string>::const_iterator i = rule_identifiers.begin();
       i != rule_identifiers.end(); ++i) {
    WebRequestRule::GlobalRuleId rule_id(extension_id, *i);

    // Skip unknown rules.
    RulesMap::iterator webrequest_rules_entry = webrequest_rules_.find(rule_id);
    if (webrequest_rules_entry == webrequest_rules_.end())
      continue;

    // Remove all triggers but collect their IDs.
    URLMatcherConditionSet::Vector condition_sets;
    WebRequestRule* rule = webrequest_rules_entry->second.get();
    rule->conditions().GetURLMatcherConditionSets(&condition_sets);
    for (URLMatcherConditionSet::Vector::iterator j = condition_sets.begin();
         j != condition_sets.end(); ++j) {
      remove_from_url_matcher.push_back((*j)->id());
      rule_triggers_.erase((*j)->id());
    }

    // Remove reference to actual rule.
    webrequest_rules_.erase(webrequest_rules_entry);
  }

  // Clear URLMatcher based on condition_set_ids that are not needed any more.
  url_matcher_.RemoveConditionSets(remove_from_url_matcher);

  return "";
}

std::string WebRequestRulesRegistry::RemoveAllRulesImpl(
    const std::string& extension_id) {
  // Search all identifiers of rules that belong to extension |extension_id|.
  std::vector<std::string> rule_identifiers;
  for (RulesMap::iterator i = webrequest_rules_.begin();
       i != webrequest_rules_.end(); ++i) {
    const WebRequestRule::GlobalRuleId& global_rule_id = i->first;
    if (global_rule_id.first == extension_id)
      rule_identifiers.push_back(global_rule_id.second);
  }

  return RemoveRulesImpl(extension_id, rule_identifiers);
}

content::BrowserThread::ID WebRequestRulesRegistry::GetOwnerThread() const {
  return content::BrowserThread::IO;
}

bool WebRequestRulesRegistry::IsEmpty() const {
  return rule_triggers_.empty() && webrequest_rules_.empty() &&
      url_matcher_.IsEmpty();
}

WebRequestRulesRegistry::~WebRequestRulesRegistry() {}

base::Time WebRequestRulesRegistry::GetExtensionInstallationTime(
    const std::string& extension_id) const {
  if (!extension_info_map_.get())  // May be NULL during testing.
    return base::Time();

  return extension_info_map_->GetInstallTime(extension_id);
}

}  // namespace extensions
