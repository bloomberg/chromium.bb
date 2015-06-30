// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_RULE_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_RULE_H__

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_action_set.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_set.h"
#include "components/url_matcher/url_matcher.h"
#include "extensions/common/api/events.h"
#include "extensions/common/extension.h"

namespace base {
class Value;
}

namespace content {
class BrowserContext;
}

namespace extensions {

// Representation of a rule of a declarative API:
// https://developer.chrome.com/beta/extensions/events.html#declarative.
// Generally a RulesRegistry will hold a collection of Rules for a given
// declarative API and contain the logic for matching and applying them.
class DeclarativeContentRule {
 public:
  using ExtensionIdRuleIdPair = std::pair<ExtensionId, std::string>;

  DeclarativeContentRule(const ExtensionIdRuleIdPair& id,
                         const std::vector<std::string>& tags,
                         base::Time extension_installation_time,
                         scoped_ptr<DeclarativeContentConditionSet> conditions,
                         scoped_ptr<DeclarativeContentActionSet> actions,
                         int priority);
  ~DeclarativeContentRule();

  // Creates a DeclarativeContentRule for |extension| given a json definition.
  // The format of each condition and action's json is up to the specific
  // ContentCondition and ContentAction.  |extension| may be NULL in tests.
  //
  // Before constructing the final rule, calls check_consistency(conditions,
  // actions, error) and returns NULL if it fails.  Pass NULL if no consistency
  // check is needed.  If |error| is empty, the translation was successful and
  // the returned rule is internally consistent.
  static scoped_ptr<DeclarativeContentRule> Create(
      url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
      content::BrowserContext* browser_context,
      const Extension* extension,
      base::Time extension_installation_time,
      linked_ptr<core_api::events::Rule> rule,
      std::string* error);

  const ExtensionIdRuleIdPair& id() const { return id_; }
  const std::vector<std::string>& tags() const { return tags_; }
  const std::string& extension_id() const { return id_.first; }
  const DeclarativeContentConditionSet& conditions() const {
    return *conditions_;
  }
  const DeclarativeContentActionSet& actions() const { return *actions_; }
  int priority() const { return priority_; }

  // Calls actions().Apply(extension_id(), extension_installation_time_,
  // apply_info). This function should only be called when the conditions_ are
  // fulfilled (from a semantic point of view; no harm is done if this function
  // is called at other times for testing purposes).
  void Apply(ContentAction::ApplyInfo* apply_info) const;

 private:
  ExtensionIdRuleIdPair id_;
  std::vector<std::string> tags_;
  base::Time extension_installation_time_;  // For precedences of rules.
  scoped_ptr<DeclarativeContentConditionSet> conditions_;
  scoped_ptr<DeclarativeContentActionSet> actions_;
  int priority_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentRule);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_RULE_H__
