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
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/extension.h"
#include "extensions/common/error_utils.h"
#include "net/url_request/url_request.h"

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
    scoped_ptr<RulesRegistryWithCache::RuleStorageOnUI>* ui_part)
    : RulesRegistryWithCache((ui_part ? profile : NULL),
                             declarative_webrequest_constants::kOnRequest,
                             content::BrowserThread::IO,
                             true /*log_storage_init_delay*/,
                             ui_part),
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
    const ExtensionInfoMap* extension_info_map,
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
    const WebRequestRule* rule = webrequest_rules_[rule_id].get();
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
  base::Time extension_installation_time =
      GetExtensionInstallationTime(extension_id);

  std::string error;
  RulesMap new_webrequest_rules;
  const Extension* extension =
      extension_info_map_->extensions().GetByID(extension_id);

  for (std::vector<linked_ptr<RulesRegistry::Rule> >::const_iterator rule =
       rules.begin(); rule != rules.end(); ++rule) {
    WebRequestRule::GlobalRuleId rule_id(extension_id, *(*rule)->id);
    DCHECK(webrequest_rules_.find(rule_id) == webrequest_rules_.end());

    scoped_ptr<WebRequestRule> webrequest_rule(WebRequestRule::Create(
        url_matcher_.condition_factory(),
        extension_id, extension_installation_time, *rule,
        base::Bind(&Checker, base::Unretained(extension)),
        &error));
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

  // Register url patterns in |url_matcher_| and
  // |rules_with_untriggered_conditions_|.
  URLMatcherConditionSet::Vector all_new_condition_sets;
  for (RulesMap::iterator i = new_webrequest_rules.begin();
       i != new_webrequest_rules.end(); ++i) {
    i->second->conditions().GetURLMatcherConditionSets(&all_new_condition_sets);
    if (i->second->conditions().HasConditionsWithoutUrls())
      rules_with_untriggered_conditions_.insert(i->second.get());
  }
  url_matcher_.AddConditionSets(all_new_condition_sets);

  ClearCacheOnNavigation();

  if (profile_id_ && !webrequest_rules_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
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

    rules_with_untriggered_conditions_.erase(rule);

    // Removes the owning references to (and thus deletes) the rule.
    webrequest_rules_.erase(webrequest_rules_entry);
  }

  // Clear URLMatcher based on condition_set_ids that are not needed any more.
  url_matcher_.RemoveConditionSets(remove_from_url_matcher);

  ClearCacheOnNavigation();

  return std::string();
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

  // No need to call ClearCacheOnNavigation() here because RemoveRulesImpl
  // takes care of that.
  return RemoveRulesImpl(extension_id, rule_identifiers);
}

bool WebRequestRulesRegistry::IsEmpty() const {
  return rule_triggers_.empty() && webrequest_rules_.empty() &&
      url_matcher_.IsEmpty();
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
  if (extension->HasEffectiveAccessToAllHosts())
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
