// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rules_registry.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "chrome/browser/extensions/api/web_request/web_request_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/url_request/url_request.h"

using url_matcher::URLMatcherConditionSet;

namespace {

const char kActionCannotBeExecuted[] = "The action '*' can never be executed "
    "because there are is no time in the request life-cycle during which the "
    "conditions can be checked and the action can possibly be executed.";

const char kAllURLsPermissionNeeded[] =
    "To execute the action '*', you need to request host permission for all "
    "hosts.";

}  // namespace

namespace extensions {

WebRequestRulesRegistry::WebRequestRulesRegistry(
    Profile* profile,
    RulesCacheDelegate* cache_delegate,
    const WebViewKey& webview_key)
    : RulesRegistry(profile,
                    declarative_webrequest_constants::kOnRequest,
                    content::BrowserThread::IO,
                    cache_delegate,
                    webview_key),
      profile_id_(profile) {
  if (profile)
    extension_info_map_ = ExtensionSystem::Get(profile)->info_map();
}

std::set<const WebRequestRule*> WebRequestRulesRegistry::GetMatches(
    const WebRequestData& request_data_without_ids) const {
  RuleSet result;

  WebRequestDataWithMatchIds request_data(&request_data_without_ids);
  request_data.url_match_ids = url_matcher_.MatchURL(
      request_data.data->request->url());
  request_data.first_party_url_match_ids = url_matcher_.MatchURL(
      request_data.data->request->first_party_for_cookies());

  // 1st phase -- add all rules with some conditions without UrlFilter
  // attributes.
  for (RuleSet::const_iterator it = rules_with_untriggered_conditions_.begin();
       it != rules_with_untriggered_conditions_.end(); ++it) {
    if ((*it)->conditions().IsFulfilled(-1, request_data))
      result.insert(*it);
  }

  // 2nd phase -- add all rules with some conditions triggered by URL matches.
  AddTriggeredRules(request_data.url_match_ids, request_data, &result);
  AddTriggeredRules(request_data.first_party_url_match_ids,
                    request_data, &result);

  return result;
}

std::list<LinkedPtrEventResponseDelta> WebRequestRulesRegistry::CreateDeltas(
    const InfoMap* extension_info_map,
    const WebRequestData& request_data,
    bool crosses_incognito) {
  if (webrequest_rules_.empty())
    return std::list<LinkedPtrEventResponseDelta>();

  std::set<const WebRequestRule*> matches = GetMatches(request_data);

  // Sort all matching rules by their priority so that they can be processed
  // in decreasing order.
  typedef std::pair<WebRequestRule::Priority, WebRequestRule::GlobalRuleId>
      PriorityRuleIdPair;
  std::vector<PriorityRuleIdPair> ordered_matches;
  ordered_matches.reserve(matches.size());
  for (std::set<const WebRequestRule*>::iterator i = matches.begin();
       i != matches.end(); ++i) {
    ordered_matches.push_back(make_pair((*i)->priority(), (*i)->id()));
  }
  // Sort from rbegin to rend in order to get descending priority order.
  std::sort(ordered_matches.rbegin(), ordered_matches.rend());

  // Build a map that maps each extension id to the minimum required priority
  // for rules of that extension. Initially, this priority is -infinite and
  // will be increased when the rules are processed and raise the bar via
  // WebRequestIgnoreRulesActions.
  typedef std::string ExtensionId;
  typedef std::map<ExtensionId, WebRequestRule::Priority> MinPriorities;
  typedef std::map<ExtensionId, std::set<std::string> > IgnoreTags;
  MinPriorities min_priorities;
  IgnoreTags ignore_tags;
  for (std::vector<PriorityRuleIdPair>::iterator i = ordered_matches.begin();
       i != ordered_matches.end(); ++i) {
    const WebRequestRule::GlobalRuleId& rule_id = i->second;
    const ExtensionId& extension_id = rule_id.first;
    min_priorities[extension_id] = std::numeric_limits<int>::min();
  }

  // Create deltas until we have passed the minimum priority.
  std::list<LinkedPtrEventResponseDelta> result;
  for (std::vector<PriorityRuleIdPair>::iterator i = ordered_matches.begin();
       i != ordered_matches.end(); ++i) {
    const WebRequestRule::Priority priority_of_rule = i->first;
    const WebRequestRule::GlobalRuleId& rule_id = i->second;
    const ExtensionId& extension_id = rule_id.first;
    const WebRequestRule* rule =
        webrequest_rules_[rule_id.first][rule_id.second].get();
    CHECK(rule);

    // Skip rule if a previous rule of this extension instructed to ignore
    // all rules with a lower priority than min_priorities[extension_id].
    int current_min_priority = min_priorities[extension_id];
    if (priority_of_rule < current_min_priority)
      continue;

    if (!rule->tags().empty() && !ignore_tags[extension_id].empty()) {
      bool ignore_rule = false;
      const WebRequestRule::Tags& tags = rule->tags();
      for (WebRequestRule::Tags::const_iterator i = tags.begin();
           !ignore_rule && i != tags.end();
           ++i) {
        ignore_rule |= ContainsKey(ignore_tags[extension_id], *i);
      }
      if (ignore_rule)
        continue;
    }

    std::list<LinkedPtrEventResponseDelta> rule_result;
    WebRequestAction::ApplyInfo apply_info = {
      extension_info_map, request_data, crosses_incognito, &rule_result,
      &ignore_tags[extension_id]
    };
    rule->Apply(&apply_info);
    result.splice(result.begin(), rule_result);

    min_priorities[extension_id] = std::max(current_min_priority,
                                            rule->GetMinimumPriority());
  }
  return result;
}

std::string WebRequestRulesRegistry::AddRulesImpl(
    const std::string& extension_id,
    const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) {
  typedef std::pair<WebRequestRule::RuleId, linked_ptr<WebRequestRule> >
      IdRulePair;
  typedef std::vector<IdRulePair> RulesVector;

  base::Time extension_installation_time =
      GetExtensionInstallationTime(extension_id);

  std::string error;
  RulesVector new_webrequest_rules;
  new_webrequest_rules.reserve(rules.size());
  const Extension* extension =
      extension_info_map_->extensions().GetByID(extension_id);
  RulesMap& registered_rules = webrequest_rules_[extension_id];

  for (std::vector<linked_ptr<RulesRegistry::Rule> >::const_iterator rule =
       rules.begin(); rule != rules.end(); ++rule) {
    const WebRequestRule::RuleId& rule_id(*(*rule)->id);
    DCHECK(registered_rules.find(rule_id) == registered_rules.end());

    scoped_ptr<WebRequestRule> webrequest_rule(WebRequestRule::Create(
        url_matcher_.condition_factory(),
        extension, extension_installation_time, *rule,
        base::Bind(&Checker, base::Unretained(extension)),
        &error));
    if (!error.empty()) {
      // We don't return here, because we want to clear temporary
      // condition sets in the url_matcher_.
      break;
    }

    new_webrequest_rules.push_back(
        IdRulePair(rule_id, make_linked_ptr(webrequest_rule.release())));
  }

  if (!error.empty()) {
    // Clean up temporary condition sets created during rule creation.
    url_matcher_.ClearUnusedConditionSets();
    return error;
  }

  // Wohoo, everything worked fine.
  registered_rules.insert(new_webrequest_rules.begin(),
                          new_webrequest_rules.end());

  // Create the triggers.
  for (RulesVector::const_iterator i = new_webrequest_rules.begin();
       i != new_webrequest_rules.end(); ++i) {
    URLMatcherConditionSet::Vector url_condition_sets;
    const WebRequestConditionSet& conditions = i->second->conditions();
    conditions.GetURLMatcherConditionSets(&url_condition_sets);
    for (URLMatcherConditionSet::Vector::iterator j =
         url_condition_sets.begin(); j != url_condition_sets.end(); ++j) {
      rule_triggers_[(*j)->id()] = i->second.get();
    }
  }

  // Register url patterns in |url_matcher_| and
  // |rules_with_untriggered_conditions_|.
  URLMatcherConditionSet::Vector all_new_condition_sets;
  for (RulesVector::const_iterator i = new_webrequest_rules.begin();
       i != new_webrequest_rules.end(); ++i) {
    i->second->conditions().GetURLMatcherConditionSets(&all_new_condition_sets);
    if (i->second->conditions().HasConditionsWithoutUrls())
      rules_with_untriggered_conditions_.insert(i->second.get());
  }
  url_matcher_.AddConditionSets(all_new_condition_sets);

  ClearCacheOnNavigation();

  if (profile_id_ && !registered_rules.empty()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&extension_web_request_api_helpers::NotifyWebRequestAPIUsed,
                   profile_id_, make_scoped_refptr(extension)));
  }

  return std::string();
}

std::string WebRequestRulesRegistry::RemoveRulesImpl(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  // URLMatcherConditionSet IDs that can be removed from URLMatcher.
  std::vector<URLMatcherConditionSet::ID> remove_from_url_matcher;
  RulesMap& registered_rules = webrequest_rules_[extension_id];

  for (std::vector<std::string>::const_iterator i = rule_identifiers.begin();
       i != rule_identifiers.end(); ++i) {
    // Skip unknown rules.
    RulesMap::iterator webrequest_rules_entry = registered_rules.find(*i);
    if (webrequest_rules_entry == registered_rules.end())
      continue;

    // Remove all triggers but collect their IDs.
    CleanUpAfterRule(webrequest_rules_entry->second.get(),
                     &remove_from_url_matcher);

    // Removes the owning references to (and thus deletes) the rule.
    registered_rules.erase(webrequest_rules_entry);
  }
  if (registered_rules.empty())
    webrequest_rules_.erase(extension_id);

  // Clear URLMatcher based on condition_set_ids that are not needed any more.
  url_matcher_.RemoveConditionSets(remove_from_url_matcher);

  ClearCacheOnNavigation();

  return std::string();
}

std::string WebRequestRulesRegistry::RemoveAllRulesImpl(
    const std::string& extension_id) {
  // First we get out all URLMatcherConditionSets and remove the rule references
  // from |rules_with_untriggered_conditions_|.
  std::vector<URLMatcherConditionSet::ID> remove_from_url_matcher;
  for (RulesMap::const_iterator it = webrequest_rules_[extension_id].begin();
       it != webrequest_rules_[extension_id].end();
       ++it) {
    CleanUpAfterRule(it->second.get(), &remove_from_url_matcher);
  }
  url_matcher_.RemoveConditionSets(remove_from_url_matcher);

  webrequest_rules_.erase(extension_id);
  ClearCacheOnNavigation();
  return std::string();
}

void WebRequestRulesRegistry::CleanUpAfterRule(
    const WebRequestRule* rule,
    std::vector<URLMatcherConditionSet::ID>* remove_from_url_matcher) {
  URLMatcherConditionSet::Vector condition_sets;
  rule->conditions().GetURLMatcherConditionSets(&condition_sets);
  for (URLMatcherConditionSet::Vector::iterator j = condition_sets.begin();
       j != condition_sets.end();
       ++j) {
    remove_from_url_matcher->push_back((*j)->id());
    rule_triggers_.erase((*j)->id());
  }
  rules_with_untriggered_conditions_.erase(rule);
}

bool WebRequestRulesRegistry::IsEmpty() const {
  // Easy first.
  if (!rule_triggers_.empty() && url_matcher_.IsEmpty())
    return false;

  // Now all the registered rules for each extensions.
  for (std::map<WebRequestRule::ExtensionId, RulesMap>::const_iterator it =
           webrequest_rules_.begin();
       it != webrequest_rules_.end();
       ++it) {
    if (!it->second.empty())
      return false;
  }
  return true;
}

WebRequestRulesRegistry::~WebRequestRulesRegistry() {}

base::Time WebRequestRulesRegistry::GetExtensionInstallationTime(
    const std::string& extension_id) const {
  return extension_info_map_->GetInstallTime(extension_id);
}

void WebRequestRulesRegistry::ClearCacheOnNavigation() {
  extension_web_request_api_helpers::ClearCacheOnNavigation();
}

// static
bool WebRequestRulesRegistry::Checker(const Extension* extension,
                                      const WebRequestConditionSet* conditions,
                                      const WebRequestActionSet* actions,
                                      std::string* error) {
  return (StageChecker(conditions, actions, error) &&
          HostPermissionsChecker(extension, actions, error));
}

// static
bool WebRequestRulesRegistry::HostPermissionsChecker(
    const Extension* extension,
    const WebRequestActionSet* actions,
    std::string* error) {
  if (extension->permissions_data()->HasEffectiveAccessToAllHosts())
    return true;

  // Without the permission for all URLs, actions with the STRATEGY_DEFAULT
  // should not be registered, they would never be able to execute.
  for (WebRequestActionSet::Actions::const_iterator action_iter =
           actions->actions().begin();
       action_iter != actions->actions().end();
       ++action_iter) {
    if ((*action_iter)->host_permissions_strategy() ==
        WebRequestAction::STRATEGY_DEFAULT) {
      *error = ErrorUtils::FormatErrorMessage(kAllURLsPermissionNeeded,
                                              (*action_iter)->GetName());
      return false;
    }
  }
  return true;
}

// static
bool WebRequestRulesRegistry::StageChecker(
    const WebRequestConditionSet* conditions,
    const WebRequestActionSet* actions,
    std::string* error) {
  // Actions and conditions can be checked and executed in specific stages
  // of each web request. A rule is inconsistent if there is an action that
  // can only be triggered in stages in which no condition can be evaluated.

  // In which stages there are conditions to evaluate.
  int condition_stages = 0;
  for (WebRequestConditionSet::Conditions::const_iterator condition_iter =
           conditions->conditions().begin();
       condition_iter != conditions->conditions().end();
       ++condition_iter) {
    condition_stages |= (*condition_iter)->stages();
  }

  for (WebRequestActionSet::Actions::const_iterator action_iter =
           actions->actions().begin();
       action_iter != actions->actions().end();
       ++action_iter) {
    // Test the intersection of bit masks, this is intentionally & and not &&.
    if ((*action_iter)->stages() & condition_stages)
      continue;
    // We only get here if no matching condition was found.
    *error = ErrorUtils::FormatErrorMessage(kActionCannotBeExecuted,
                                            (*action_iter)->GetName());
    return false;
  }
  return true;
}
void WebRequestRulesRegistry::AddTriggeredRules(
    const URLMatches& url_matches,
    const WebRequestCondition::MatchData& request_data,
    RuleSet* result) const {
  for (URLMatches::const_iterator url_match = url_matches.begin();
       url_match != url_matches.end(); ++url_match) {
    RuleTriggers::const_iterator rule_trigger = rule_triggers_.find(*url_match);
    CHECK(rule_trigger != rule_triggers_.end());
    if (!ContainsKey(*result, rule_trigger->second) &&
        rule_trigger->second->conditions().IsFulfilled(*url_match,
                                                       request_data))
      result->insert(rule_trigger->second);
  }
}

}  // namespace extensions
