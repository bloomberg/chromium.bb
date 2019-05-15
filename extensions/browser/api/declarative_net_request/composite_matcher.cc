// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/composite_matcher.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "extensions/browser/api/declarative_net_request/utils.h"

namespace extensions {
namespace declarative_net_request {

namespace {

bool AreIDsUnique(const CompositeMatcher::MatcherList& matchers) {
  std::set<size_t> ids;
  for (const auto& matcher : matchers) {
    bool did_insert = ids.insert(matcher->id()).second;
    if (!did_insert)
      return false;
  }

  return true;
}

bool AreSortedPrioritiesUnique(const CompositeMatcher::MatcherList& matchers) {
  base::Optional<size_t> previous_priority;
  for (const auto& matcher : matchers) {
    if (matcher->priority() == previous_priority)
      return false;
    previous_priority = matcher->priority();
  }

  return true;
}

bool HasMatchingAllowRule(const RulesetMatcher* matcher,
                          const RequestParams& params) {
  if (!params.allow_rule_cache.contains(matcher))
    params.allow_rule_cache[matcher] = matcher->HasMatchingAllowRule(params);

  return params.allow_rule_cache[matcher];
}

}  // namespace

CompositeMatcher::CompositeMatcher(MatcherList matchers)
    : matchers_(std::move(matchers)) {
  SortMatchersByPriority();
  DCHECK(AreIDsUnique(matchers_));
}

CompositeMatcher::~CompositeMatcher() = default;

void CompositeMatcher::AddOrUpdateRuleset(
    std::unique_ptr<RulesetMatcher> new_matcher) {
  // A linear search is ok since the number of rulesets per extension is
  // expected to be quite small.
  auto it = std::find_if(
      matchers_.begin(), matchers_.end(),
      [&new_matcher](const std::unique_ptr<RulesetMatcher>& matcher) {
        return new_matcher->id() == matcher->id();
      });

  if (it == matchers_.end()) {
    matchers_.push_back(std::move(new_matcher));
    SortMatchersByPriority();
  } else {
    // Update the matcher. The priority for a given ID should remain the same.
    DCHECK_EQ(new_matcher->priority(), (*it)->priority());
    *it = std::move(new_matcher);
  }

  // Clear the renderers' cache so that they take the updated rules into
  // account.
  ClearRendererCacheOnNavigation();
  has_any_extra_headers_matcher_.reset();
}

bool CompositeMatcher::ShouldBlockRequest(const RequestParams& params) const {
  // TODO(karandeepb): change this to report time in micro-seconds.
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Extensions.DeclarativeNetRequest.ShouldBlockRequestTime."
      "SingleExtension");

  for (const auto& matcher : matchers_) {
    if (HasMatchingAllowRule(matcher.get(), params))
      return false;
    if (matcher->HasMatchingBlockRule(params))
      return true;
  }

  return false;
}

bool CompositeMatcher::ShouldRedirectRequest(const RequestParams& params,
                                             GURL* redirect_url) const {
  // TODO(karandeepb): change this to report time in micro-seconds.
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Extensions.DeclarativeNetRequest.ShouldRedirectRequestTime."
      "SingleExtension");

  for (const auto& matcher : matchers_) {
    if (HasMatchingAllowRule(matcher.get(), params))
      return false;
    if (matcher->HasMatchingRedirectRule(params, redirect_url))
      return true;
  }

  return false;
}

uint8_t CompositeMatcher::GetRemoveHeadersMask(const RequestParams& params,
                                               uint8_t current_mask) const {
  uint8_t mask = current_mask;
  for (const auto& matcher : matchers_) {
    // The allow rule will override lower priority remove header rules.
    if (HasMatchingAllowRule(matcher.get(), params))
      return mask;
    mask |= matcher->GetRemoveHeadersMask(params, mask);
  }
  return mask;
}

bool CompositeMatcher::HasAnyExtraHeadersMatcher() const {
  if (!has_any_extra_headers_matcher_.has_value())
    has_any_extra_headers_matcher_ = ComputeHasAnyExtraHeadersMatcher();
  return has_any_extra_headers_matcher_.value();
}

bool CompositeMatcher::ComputeHasAnyExtraHeadersMatcher() const {
  for (const auto& matcher : matchers_) {
    if (matcher->IsExtraHeadersMatcher())
      return true;
  }
  return false;
}

void CompositeMatcher::SortMatchersByPriority() {
  std::sort(matchers_.begin(), matchers_.end(),
            [](const std::unique_ptr<RulesetMatcher>& a,
               const std::unique_ptr<RulesetMatcher>& b) {
              return a->priority() > b->priority();
            });
  DCHECK(AreSortedPrioritiesUnique(matchers_));
}

}  // namespace declarative_net_request
}  // namespace extensions
