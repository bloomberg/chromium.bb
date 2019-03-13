// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/composite_matcher.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/metrics/histogram_macros.h"

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

}  // namespace

CompositeMatcher::CompositeMatcher(MatcherList matchers)
    : matchers_(std::move(matchers)) {
  // Sort in descending order of priority.
  std::sort(matchers_.begin(), matchers_.end(),
            [](const std::unique_ptr<RulesetMatcher>& a,
               const std::unique_ptr<RulesetMatcher>& b) {
              return a->priority() > b->priority();
            });

  DCHECK(AreIDsUnique(matchers_));
  DCHECK(AreSortedPrioritiesUnique(matchers_));
}

CompositeMatcher::~CompositeMatcher() = default;

bool CompositeMatcher::ShouldBlockRequest(const RequestParams& params) const {
  // TODO(karandeepb): change this to report time in micro-seconds.
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Extensions.DeclarativeNetRequest.ShouldBlockRequestTime."
      "SingleExtension");

  for (const auto& matcher : matchers_) {
    if (matcher->HasMatchingAllowRule(params))
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
    if (matcher->HasMatchingRedirectRule(params, redirect_url))
      return true;
  }

  return false;
}

}  // namespace declarative_net_request
}  // namespace extensions
