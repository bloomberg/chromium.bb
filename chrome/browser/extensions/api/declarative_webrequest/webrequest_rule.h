// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULE_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/api/declarative/rules_registry.h"

namespace extensions {
class URLMatcherConditionFactory;
class WebRequestConditionSet;
class WebRequestActionSet;
}

namespace extensions {

// Representation of a rule of the declarative Web Request API
class WebRequestRule {
 public:
  typedef std::string ExtensionId;
  typedef std::string RuleId;
  typedef std::pair<ExtensionId, RuleId> GlobalRuleId;

  WebRequestRule(const GlobalRuleId& id,
                 scoped_ptr<WebRequestConditionSet> conditions,
                 scoped_ptr<WebRequestActionSet> actions);
  virtual ~WebRequestRule();

  // If |error| is empty, the translation was successful and the returned
  // rule is internally consistent.
  static scoped_ptr<WebRequestRule> Create(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const std::string& extension_id,
      linked_ptr<RulesRegistry::Rule> rule,
      std::string* error);

  const GlobalRuleId& id() const { return id_; }
  const WebRequestConditionSet& conditions() const { return *conditions_; }
  const WebRequestActionSet& actions() const { return *actions_; }

 private:
  // Checks whether the set of |conditions| and |actions| are consistent,
  // meaning for example that we do not allow combining an |action| that needs
  // to be executed before the |condition| can be fulfilled.
  // Returns true in case of consistency and MUST set |error| otherwise.
  static bool CheckConsistency(WebRequestConditionSet* conditions,
                               WebRequestActionSet* actions,
                               std::string* error);

  GlobalRuleId id_;
  scoped_ptr<WebRequestConditionSet> conditions_;
  scoped_ptr<WebRequestActionSet> actions_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestRule);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULE_H_
