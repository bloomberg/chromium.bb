// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_set.h"

namespace extensions {

DeclarativeContentConditionSet::DeclarativeContentConditionSet(
    const Conditions& conditions,
    const URLMatcherIdToCondition& match_id_to_condition,
    const std::vector<const ContentCondition*>& conditions_without_urls)
    : match_id_to_condition_(match_id_to_condition),
      conditions_(conditions),
      conditions_without_urls_(conditions_without_urls) {}

DeclarativeContentConditionSet::~DeclarativeContentConditionSet() {
}

bool DeclarativeContentConditionSet::IsFulfilled(
    url_matcher::URLMatcherConditionSet::ID url_match_trigger,
    const RendererContentMatchData& match_data) const {
  if (url_match_trigger == -1) {
    // Invalid trigger -- indication that we should only check conditions
    // without URL attributes.
    for (const ContentCondition* condition : conditions_without_urls_) {
      if (condition->IsFulfilled(match_data))
        return true;
    }
    return false;
  }

  URLMatcherIdToCondition::const_iterator triggered =
      match_id_to_condition_.find(url_match_trigger);
  return (triggered != match_id_to_condition_.end() &&
          triggered->second->IsFulfilled(match_data));
}

void DeclarativeContentConditionSet::GetURLMatcherConditionSets(
    url_matcher::URLMatcherConditionSet::Vector* condition_sets) const {
  for (const linked_ptr<const ContentCondition>& condition : conditions_)
    condition->GetURLMatcherConditionSets(condition_sets);
}

}  // namespace extensions
