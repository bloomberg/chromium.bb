// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_SET_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_SET_H__

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/api/declarative_content/content_condition.h"
#include "components/url_matcher/url_matcher.h"

namespace extensions {

// This class stores a set of conditions that may be part of a
// DeclarativeContentRule.  If any condition is fulfilled, the Actions of the
// DeclarativeContentRule can be triggered.
class DeclarativeContentConditionSet {
 public:
  using Conditions = std::vector<linked_ptr<const ContentCondition>>;
  using const_iterator = Conditions::const_iterator;
  using URLMatcherIdToCondition =
      std::map<url_matcher::URLMatcherConditionSet::ID,
               const ContentCondition*>;

  DeclarativeContentConditionSet(
      const Conditions& conditions,
      const URLMatcherIdToCondition& match_id_to_condition,
      const std::vector<const ContentCondition*>& conditions_without_urls);

  ~DeclarativeContentConditionSet();

  const Conditions& conditions() const {
    return conditions_;
  }

  const_iterator begin() const { return conditions_.begin(); }
  const_iterator end() const { return conditions_.end(); }

  // If |url_match_trigger| is not -1, this function looks for a condition
  // with this URLMatcherConditionSet, and forwards to that condition's
  // IsFulfilled(|match_data|). If there is no such condition, then false is
  // returned. If |url_match_trigger| is -1, this function returns whether any
  // of the conditions without URL attributes is satisfied.
  bool IsFulfilled(url_matcher::URLMatcherConditionSet::ID url_match_trigger,
                   const RendererContentMatchData& match_data) const;

  // Appends the URLMatcherConditionSet from all conditions to |condition_sets|.
  void GetURLMatcherConditionSets(
      url_matcher::URLMatcherConditionSet::Vector* condition_sets) const;

  // Returns whether there are some conditions without UrlFilter attributes.
  bool HasConditionsWithoutUrls() const {
    return !conditions_without_urls_.empty();
  }

 private:
  const URLMatcherIdToCondition match_id_to_condition_;
  const Conditions conditions_;
  const std::vector<const ContentCondition*> conditions_without_urls_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentConditionSet);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_SET_H__
