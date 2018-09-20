// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_precondition.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Static
std::unique_ptr<ScriptPrecondition> ScriptPrecondition::FromProto(
    const ScriptPreconditionProto& proto) {
  std::vector<std::vector<std::string>> elements_exist;
  for (const auto& element : proto.elements_exist()) {
    std::vector<std::string> selectors;
    for (const auto& selector : element.selectors()) {
      selectors.emplace_back(selector);
    }
    elements_exist.emplace_back(selectors);
  }

  std::set<std::string> domain_match;
  for (const auto& domain : proto.domain()) {
    domain_match.emplace(domain);
  }

  std::vector<std::unique_ptr<re2::RE2>> path_pattern;
  for (const auto& pattern : proto.path_pattern()) {
    auto re = std::make_unique<re2::RE2>(pattern);
    if (re->error_code() != re2::RE2::NoError) {
      LOG(ERROR) << "Invalid regexp in script precondition '" << pattern;
      return nullptr;
    }
    path_pattern.emplace_back(std::move(re));
  }

  std::vector<ScriptParameterMatchProto> parameter_match;
  for (const auto& match : proto.script_parameter_match()) {
    parameter_match.emplace_back(match);
  }

  // TODO(crbug.com/806868): Detect unknown or unsupported conditions and reject
  // them.
  return std::make_unique<ScriptPrecondition>(
      elements_exist, domain_match, std::move(path_pattern), parameter_match);
}

ScriptPrecondition::~ScriptPrecondition() {}

void ScriptPrecondition::Check(
    WebController* web_controller,
    const std::map<std::string, std::string>& parameters,
    base::OnceCallback<void(bool)> callback) {
  const GURL& url = web_controller->GetUrl();
  if (!MatchDomain(url) || !MatchPath(url) || !MatchParameters(parameters)) {
    std::move(callback).Run(false);
    return;
  }

  if (elements_exist_.empty()) {
    std::move(callback).Run(true);
    return;
  }

  check_preconditions_callback_ = std::move(callback);
  pending_elements_exist_check_ = 0;
  for (const auto& element : elements_exist_) {
    pending_elements_exist_check_++;
    web_controller->ElementExists(
        element, base::BindOnce(&ScriptPrecondition::OnCheckElementExists,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

ScriptPrecondition::ScriptPrecondition(
    const std::vector<std::vector<std::string>>& elements_exist,
    const std::set<std::string>& domain_match,
    std::vector<std::unique_ptr<re2::RE2>> path_pattern,
    const std::vector<ScriptParameterMatchProto>& parameter_match)
    : elements_exist_(elements_exist),
      domain_match_(domain_match),
      path_pattern_(std::move(path_pattern)),
      parameter_match_(parameter_match),
      weak_ptr_factory_(this) {}

void ScriptPrecondition::OnCheckElementExists(bool result) {
  pending_elements_exist_check_--;
  // Return false early if there is a check failed.
  if (!result) {
    std::move(check_preconditions_callback_).Run(false);
    return;
  }

  // Return true if all checks have been completed and there is no failed check.
  if (!pending_elements_exist_check_ && check_preconditions_callback_) {
    std::move(check_preconditions_callback_).Run(true);
  }
}

bool ScriptPrecondition::MatchDomain(const GURL& url) const {
  if (domain_match_.empty())
    return true;

  return domain_match_.find(url.host()) != domain_match_.end();
}

bool ScriptPrecondition::MatchPath(const GURL& url) const {
  if (path_pattern_.empty()) {
    return true;
  }

  const std::string path = url.path();
  for (auto& regexp : path_pattern_) {
    if (regexp->Match(path, 0, path.size(), re2::RE2::UNANCHORED, NULL, 0)) {
      return true;
    }
  }
  return false;
}

bool ScriptPrecondition::MatchParameters(
    const std::map<std::string, std::string>& parameters) const {
  for (const auto& match : parameter_match_) {
    auto iter = parameters.find(match.name());
    if (match.exists()) {
      // parameter must exist and optionally have a specific value
      if (iter == parameters.end())
        return false;

      if (!match.value_equals().empty() && iter->second != match.value_equals())
        return false;

    } else {
      // parameter must not exist
      if (iter != parameters.end())
        return false;
    }
  }
  return true;
}
}  // namespace autofill_assistant.
