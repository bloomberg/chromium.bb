// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULES_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULES_REGISTRY_H_
#pragma once

#include <list>
#include <map>
#include <set>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_with_cache.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stages.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rule.h"
#include "chrome/common/extensions/matcher/url_matcher.h"

namespace extension_web_request_api_helpers {
struct EventResponseDelta;
}

namespace net {
class URLRequest;
}

namespace extensions {

class RulesRegistryService;
class WebRequestRule;

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
  WebRequestRulesRegistry();

  // TODO(battre): This will become an implementation detail, because we need
  // a way to also execute the actions of the rules.
  std::set<WebRequestRule::GlobalRuleId> GetMatches(
      net::URLRequest* request,
      RequestStages request_stage);

  // Returns which modifications should be executed on the network request
  // according to the rules registered in this registry.
  std::list<LinkedPtrEventResponseDelta> CreateDeltas(
      net::URLRequest* request, RequestStages request_stage);

  // Implementation of RulesRegistryWithCache:
  virtual std::string AddRulesImpl(
      const std::string& extension_id,
      const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) OVERRIDE;
  virtual std::string RemoveRulesImpl(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) OVERRIDE;
  virtual std::string RemoveAllRulesImpl(
      const std::string& extension_id) OVERRIDE;
  virtual content::BrowserThread::ID GetOwnerThread() const OVERRIDE;

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

 private:
  typedef std::map<URLMatcherConditionSet::ID, WebRequestRule*> RuleTriggers;
  typedef std::map<WebRequestRule::GlobalRuleId, linked_ptr<WebRequestRule> >
      RulesMap;

  virtual ~WebRequestRulesRegistry();

  // Map that tells us which WebRequestRule may match under the condition that
  // the URLMatcherConditionSet::ID was returned by the |url_matcher_|.
  RuleTriggers rule_triggers_;

  RulesMap webrequest_rules_;

  URLMatcher url_matcher_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULES_REGISTRY_H_
