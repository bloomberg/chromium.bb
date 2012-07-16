// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rule.h"

#include "base/logging.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_action.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"

namespace {
const char kInvalidActionDatatype[] = "An action of a rule set had an invalid "
    "structure that should have been caught by the JSON validator.";
}  // namespace

namespace extensions {

WebRequestRule::WebRequestRule(
    const GlobalRuleId& id,
    base::Time extension_installation_time,
    scoped_ptr<WebRequestConditionSet> conditions,
    scoped_ptr<WebRequestActionSet> actions,
    Priority priority)
    : id_(id),
      extension_installation_time_(extension_installation_time),
      conditions_(conditions.release()),
      actions_(actions.release()),
      priority_(priority) {
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
    base::Time extension_installation_time,
    linked_ptr<RulesRegistry::Rule> rule,
    std::string* error) {
  scoped_ptr<WebRequestRule> error_result;

  scoped_ptr<WebRequestConditionSet> conditions =
      WebRequestConditionSet::Create(
          url_matcher_condition_factory, rule->conditions, error);
  if (!error->empty())
    return error_result.Pass();
  CHECK(conditions.get());

  bool bad_message = false;
  scoped_ptr<WebRequestActionSet> actions =
      WebRequestActionSet::Create(rule->actions, error, &bad_message);
  if (bad_message) {
    // TODO(battre) Export concept of bad_message to caller, the extension
    // should be killed in case it is true.
    *error = kInvalidActionDatatype;
    return error_result.Pass();
  }
  if (!error->empty() || bad_message)
    return error_result.Pass();
  CHECK(actions.get());

  if (!CheckConsistency(conditions.get(), actions.get(), error)) {
    DCHECK(!error->empty());
    return error_result.Pass();
  }

  CHECK(rule->priority.get());
  int priority = *(rule->priority);

  GlobalRuleId rule_id(extension_id, *(rule->id));
  return scoped_ptr<WebRequestRule>(
      new WebRequestRule(rule_id, extension_installation_time,
                         conditions.Pass(), actions.Pass(), priority));
}

std::list<LinkedPtrEventResponseDelta> WebRequestRule::CreateDeltas(
    net::URLRequest* request,
    RequestStages request_stage,
    const OptionalRequestData& optional_request_data) const {
  return actions_->CreateDeltas(request, request_stage, optional_request_data,
      id_.first, extension_installation_time_);
}

int WebRequestRule::GetMinimumPriority() const {
  return actions_->GetMinimumPriority();
}

}  // namespace extensions
