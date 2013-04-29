// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULES_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULES_REGISTRY_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/declarative/declarative_rule.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_with_cache.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stage.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_action.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "extensions/common/matcher/url_matcher.h"

class Profile;
class WebRequestPermissions;

namespace extension_web_request_api_helpers {
struct EventResponseDelta;
}

namespace net {
class URLRequest;
}

namespace extensions {

class RulesRegistryService;

typedef linked_ptr<extension_web_request_api_helpers::EventResponseDelta>
    LinkedPtrEventResponseDelta;
typedef DeclarativeRule<WebRequestCondition, WebRequestAction> WebRequestRule;

// The WebRequestRulesRegistry is responsible for managing
// the internal representation of rules for the Declarative Web Request API.
//
// Here is the high level overview of this functionality:
//
// RulesRegistry::Rule consists of Conditions and Actions, these are
// represented as a WebRequestRule with WebRequestConditions and
// WebRequestRuleActions.
//
// WebRequestConditions represent JSON dictionaries as the following:
// {
//   'instanceType': 'URLMatcher',
//   'host_suffix': 'example.com',
//   'path_prefix': '/query',
//   'scheme': 'http'
// }
//
// The evaluation of URL related condition attributes (host_suffix, path_prefix)
// is delegated to a URLMatcher, because this is capable of evaluating many
// of such URL related condition attributes in parallel.
//
// For this, the URLRequestCondition has a URLMatcherConditionSet, which
// represents the {'host_suffix': 'example.com', 'path_prefix': '/query'} part.
// We will then ask the URLMatcher, whether a given URL
// "http://www.example.com/query/" has any matches, and the URLMatcher
// will respond with the URLMatcherConditionSet::ID. We can map this
// to the WebRequestRule and check whether also the other conditions (in this
// example 'scheme': 'http') are fulfilled.
class WebRequestRulesRegistry : public RulesRegistryWithCache {
 public:
  // For testing, |ui_part| can be NULL. In that case it constructs the
  // registry with storage functionality suspended.
  WebRequestRulesRegistry(
      Profile* profile,
      scoped_ptr<RulesRegistryWithCache::RuleStorageOnUI>* ui_part);

  // TODO(battre): This will become an implementation detail, because we need
  // a way to also execute the actions of the rules.
  std::set<const WebRequestRule*> GetMatches(
      const WebRequestData& request_data_without_ids) const;

  // Returns which modifications should be executed on the network request
  // according to the rules registered in this registry.
  std::list<LinkedPtrEventResponseDelta> CreateDeltas(
      const ExtensionInfoMap* extension_info_map,
      const WebRequestData& request_data,
      bool crosses_incognito);

  // Implementation of RulesRegistryWithCache:
  virtual std::string AddRulesImpl(
      const std::string& extension_id,
      const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) OVERRIDE;
  virtual std::string RemoveRulesImpl(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) OVERRIDE;
  virtual std::string RemoveAllRulesImpl(
      const std::string& extension_id) OVERRIDE;

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

 protected:
  virtual ~WebRequestRulesRegistry();

  // Virtual for testing:
  virtual base::Time GetExtensionInstallationTime(
      const std::string& extension_id) const;
  virtual void ClearCacheOnNavigation();

  void SetExtensionInfoMapForTesting(
      scoped_refptr<ExtensionInfoMap> extension_info_map) {
    extension_info_map_ = extension_info_map;
  }

  const std::set<const WebRequestRule*>&
  rules_with_untriggered_conditions_for_test() const {
    return rules_with_untriggered_conditions_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WebRequestRulesRegistrySimpleTest, StageChecker);
  FRIEND_TEST_ALL_PREFIXES(WebRequestRulesRegistrySimpleTest,
                           HostPermissionsChecker);

  typedef std::map<URLMatcherConditionSet::ID, WebRequestRule*> RuleTriggers;
  typedef std::map<WebRequestRule::GlobalRuleId, linked_ptr<WebRequestRule> >
      RulesMap;
  typedef std::set<URLMatcherConditionSet::ID> URLMatches;
  typedef std::set<const WebRequestRule*> RuleSet;

  // This bundles all consistency checkers. Returns true in case of consistency
  // and MUST set |error| otherwise.
  static bool Checker(const Extension* extension,
                      const WebRequestConditionSet* conditions,
                      const WebRequestActionSet* actions,
                      std::string* error);

  // Check that the |extension| has host permissions for all URLs if actions
  // requiring them are present.
  static bool HostPermissionsChecker(const Extension* extension,
                                     const WebRequestActionSet* actions,
                                     std::string* error);

  // Check that every action is applicable in the same request stage as at
  // least one condition.
  static bool StageChecker(const WebRequestConditionSet* conditions,
                           const WebRequestActionSet* actions,
                           std::string* error);

  // This is a helper function to GetMatches. Rules triggered by |url_matches|
  // get added to |result| if one of their conditions is fulfilled.
  // |request_data| gets passed to IsFulfilled of the rules' condition sets.
  void AddTriggeredRules(const URLMatches& url_matches,
                         const WebRequestCondition::MatchData& request_data,
                         RuleSet* result) const;

  // Map that tells us which WebRequestRule may match under the condition that
  // the URLMatcherConditionSet::ID was returned by the |url_matcher_|.
  RuleTriggers rule_triggers_;

  // These rules contain condition sets with conditions without URL attributes.
  // Such conditions are not triggered by URL matcher, so we need to test them
  // separately.
  std::set<const WebRequestRule*> rules_with_untriggered_conditions_;

  RulesMap webrequest_rules_;

  URLMatcher url_matcher_;

  void* profile_id_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestRulesRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULES_REGISTRY_H_
