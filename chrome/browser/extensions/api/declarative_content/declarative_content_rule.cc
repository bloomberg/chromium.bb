// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_rule.h"

namespace extensions {

DeclarativeContentRule::DeclarativeContentRule(
    const ExtensionIdRuleIdPair& id,
    const std::vector<std::string>& tags,
    base::Time extension_installation_time,
    scoped_ptr<DeclarativeContentConditionSet> conditions,
    scoped_ptr<DeclarativeContentActionSet> actions,
    int priority)
    : id_(id),
      tags_(tags),
      extension_installation_time_(extension_installation_time),
      conditions_(conditions.release()),
      actions_(actions.release()),
      priority_(priority) {
  CHECK(conditions_.get());
  CHECK(actions_.get());
}

DeclarativeContentRule::~DeclarativeContentRule() {}

// static
scoped_ptr<DeclarativeContentRule> DeclarativeContentRule::Create(
    url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
    content::BrowserContext* browser_context,
    const Extension* extension,
    base::Time extension_installation_time,
    linked_ptr<core_api::events::Rule> rule,
    std::string* error) {
  scoped_ptr<DeclarativeContentRule> error_result;

  scoped_ptr<DeclarativeContentConditionSet> conditions =
      DeclarativeContentConditionSet::Create(extension,
                                             url_matcher_condition_factory,
                                             rule->conditions, error);
  if (!error->empty())
    return error_result.Pass();
  CHECK(conditions.get());

  bool bad_message = false;
  scoped_ptr<DeclarativeContentActionSet> actions =
      DeclarativeContentActionSet::Create(browser_context, extension,
                                          rule->actions, error, &bad_message);
  if (bad_message) {
    // TODO(battre) Export concept of bad_message to caller, the extension
    // should be killed in case it is true.
    *error = "An action of a rule set had an invalid "
        "structure that should have been caught by the JSON validator.";
    return error_result.Pass();
  }
  if (!error->empty() || bad_message)
    return error_result.Pass();
  CHECK(actions.get());

  CHECK(rule->priority.get());
  int priority = *(rule->priority);

  ExtensionIdRuleIdPair id(extension->id(), *(rule->id));
  std::vector<std::string> tags = rule->tags ? *rule->tags :
      std::vector<std::string>();
  return scoped_ptr<DeclarativeContentRule>(
      new DeclarativeContentRule(id, tags, extension_installation_time,
                          conditions.Pass(), actions.Pass(), priority));
}

void DeclarativeContentRule::Apply(ContentAction::ApplyInfo* apply_info) const {
  return actions_->Apply(extension_id(),
                         extension_installation_time_,
                         apply_info);
}

}  // namespace extensions
