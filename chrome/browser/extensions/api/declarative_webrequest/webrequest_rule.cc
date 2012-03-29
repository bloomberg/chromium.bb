// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rule.h"

#include "base/logging.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_action.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"

namespace extensions {

WebRequestRule::WebRequestRule(
    const GlobalRuleId& id,
    scoped_ptr<WebRequestConditionSet> conditions,
    scoped_ptr<WebRequestActionSet> actions)
    : id_(id),
      conditions_(conditions.release()),
      actions_(actions.release()) {
  CHECK(conditions_.get());
  CHECK(actions_.get());
}

WebRequestRule::~WebRequestRule() {}

// static
bool WebRequestRule::CheckConsistency(
    WebRequestConditionSet* conditions,
    WebRequestActionSet* actions,
    std::string* error) {
  // TODO(battre): Implement this.
  return true;
}

// static
scoped_ptr<WebRequestRule> WebRequestRule::Create(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const std::string& extension_id,
    linked_ptr<RulesRegistry::Rule> rule,
    std::string* error) {
  scoped_ptr<WebRequestRule> error_result;

  scoped_ptr<WebRequestConditionSet> conditions =
      WebRequestConditionSet::Create(
          url_matcher_condition_factory, rule->conditions, error);
  if (!error->empty())
    return error_result.Pass();
  CHECK(conditions.get());

  scoped_ptr<WebRequestActionSet> actions =
      WebRequestActionSet::Create(rule->actions, error);
  if (!error->empty())
    return error_result.Pass();
  CHECK(actions.get());

  if (!CheckConsistency(conditions.get(), actions.get(), error)) {
    DCHECK(!error->empty());
    return error_result.Pass();
  }

  GlobalRuleId rule_id = make_pair(extension_id, *(rule->id));
  return scoped_ptr<WebRequestRule>(
      new WebRequestRule(rule_id, conditions.Pass(), actions.Pass()));
}

}  // namespace extensions
