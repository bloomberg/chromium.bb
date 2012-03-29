// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rules_registry.h"

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"
#include "net/url_request/url_request.h"

namespace extensions {

WebRequestRulesRegistry::WebRequestRulesRegistry() {}

WebRequestRulesRegistry::~WebRequestRulesRegistry() {}

std::set<WebRequestRule::GlobalRuleId>
WebRequestRulesRegistry::GetMatches(net::URLRequest* request) {
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
    if (rule->conditions().IsFulfilled(*url_match, request))
      result.insert(rule->id());
  }

  return result;
}

std::string WebRequestRulesRegistry::AddRulesImpl(
    const std::string& extension_id,
    const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) {
  std::string error;
  RulesMap new_webrequest_rules;

  for (std::vector<linked_ptr<RulesRegistry::Rule> >::const_iterator rule =
       rules.begin(); rule != rules.end(); ++rule) {
    WebRequestRule::GlobalRuleId rule_id(extension_id, *(*rule)->id);
    DCHECK(webrequest_rules_.find(rule_id) == webrequest_rules_.end());

    scoped_ptr<WebRequestRule> webrequest_rule(
        WebRequestRule::Create(url_matcher_.condition_factory(), extension_id,
                               *rule, &error));
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
    std::vector<URLMatcherConditionSet> url_condition_sets;
    const WebRequestConditionSet& conditions = i->second->conditions();
    conditions.GetURLMatcherConditionSets(&url_condition_sets);
    for (std::vector<URLMatcherConditionSet>::iterator j =
        url_condition_sets.begin(); j != url_condition_sets.end(); ++j) {
      rule_triggers_[j->id()] = i->second.get();
    }
  }

  // Register url patterns in url_matcher_.
  std::vector<URLMatcherConditionSet> all_new_condition_sets;
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
  return "TODO";
}

std::string WebRequestRulesRegistry::RemoveAllRulesImpl(
    const std::string& extension_id) {
  return "TODO";
}

content::BrowserThread::ID WebRequestRulesRegistry::GetOwnerThread() const {
  return content::BrowserThread::IO;
}

}  // namespace extensions
