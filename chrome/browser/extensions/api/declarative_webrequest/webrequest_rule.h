// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULE_H_
#pragma once

#include <list>
#include <vector>

#include "base/compiler_specific.h"
#include "base/time.h"
#include "chrome/browser/extensions/api/declarative/rules_registry.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stages.h"

namespace extensions {
class URLMatcherConditionFactory;
class WebRequestConditionSet;
class WebRequestActionSet;
}

namespace extension_web_request_api_helpers {
struct EventResponseDelta;
}

namespace net {
class HttpResponseHeaders;
class URLRequest;
}

namespace extensions {

typedef linked_ptr<extension_web_request_api_helpers::EventResponseDelta>
    LinkedPtrEventResponseDelta;

// Representation of a rule of the declarative Web Request API
class WebRequestRule {
 public:
  typedef std::string ExtensionId;
  typedef std::string RuleId;
  typedef std::pair<ExtensionId, RuleId> GlobalRuleId;
  typedef int Priority;

  // Container to pass additional information about requests that are not
  // available in all request stages.
  struct OptionalRequestData {
    OptionalRequestData() : original_response_headers(NULL) {}
    net::HttpResponseHeaders* original_response_headers;
  };

  WebRequestRule(const GlobalRuleId& id,
                 base::Time extension_installation_time,
                 scoped_ptr<WebRequestConditionSet> conditions,
                 scoped_ptr<WebRequestActionSet> actions,
                 Priority priority);
  virtual ~WebRequestRule();

  // If |error| is empty, the translation was successful and the returned
  // rule is internally consistent.
  static scoped_ptr<WebRequestRule> Create(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const std::string& extension_id,
      base::Time extension_installation_time,
      linked_ptr<RulesRegistry::Rule> rule,
      std::string* error);

  const GlobalRuleId& id() const { return id_; }
  const WebRequestConditionSet& conditions() const { return *conditions_; }
  const WebRequestActionSet& actions() const { return *actions_; }
  Priority priority() const { return priority_; }

  std::list<LinkedPtrEventResponseDelta> CreateDeltas(
      net::URLRequest* request,
      RequestStages request_stage,
      const OptionalRequestData& optional_request_data) const;

  // Returns the minimum priority of rules that may be evaluated after
  // this rule. Defaults to MAX_INT. Only valid if the conditions of this rule
  // are fulfilled.
  Priority GetMinimumPriority() const;

 private:
  // Checks whether the set of |conditions| and |actions| are consistent,
  // meaning for example that we do not allow combining an |action| that needs
  // to be executed before the |condition| can be fulfilled.
  // Returns true in case of consistency and MUST set |error| otherwise.
  static bool CheckConsistency(WebRequestConditionSet* conditions,
                               WebRequestActionSet* actions,
                               std::string* error);

  GlobalRuleId id_;
  base::Time extension_installation_time_;  // For precedences of rules.
  scoped_ptr<WebRequestConditionSet> conditions_;
  scoped_ptr<WebRequestActionSet> actions_;
  Priority priority_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestRule);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULE_H_
