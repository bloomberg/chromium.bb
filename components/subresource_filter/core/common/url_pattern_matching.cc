// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/url_pattern_matching.h"

#include "base/logging.h"
#include "components/subresource_filter/core/common/fuzzy_pattern_matching.h"
#include "components/subresource_filter/core/common/knuth_morris_pratt.h"
#include "components/subresource_filter/core/common/string_splitter.h"
#include "components/subresource_filter/core/common/url_pattern.h"

namespace subresource_filter {

namespace {

bool IsWildcard(char c) {
  return c == '*';
}

size_t FindFirstOccurrenceFuzzy(base::StringPiece text,
                                base::StringPiece subpattern,
                                const size_t* failure) {
  return *AllOccurrencesFuzzy(text, subpattern, failure).begin();
}

}  // namespace

void BuildFailureFunction(const UrlPattern& pattern,
                          std::vector<size_t>* failure) {
  auto subpatterns = CreateStringSplitter(pattern.url_pattern, IsWildcard);
  auto subpattern_it = subpatterns.begin();
  auto subpattern_end = subpatterns.end();

  if (subpattern_it == subpattern_end)
    return;
  if (pattern.anchor_left == proto::ANCHOR_TYPE_BOUNDARY)
    ++subpattern_it;

  while (subpattern_it != subpattern_end) {
    const base::StringPiece part = *subpattern_it++;
    DCHECK(!part.empty());
    if (pattern.anchor_right == proto::ANCHOR_TYPE_BOUNDARY &&
        subpattern_it == subpattern_end) {
      break;
    }

    if (part.find(kSeparatorPlaceholder) == base::StringPiece::npos) {
      BuildFailureFunction(part, failure);
    } else {
      // Prepend the value '1' to the failure function to let matchers
      // distinguish between the subpatterns with separator placeholders and
      // those without.
      failure->push_back(1);
      BuildFailureFunctionFuzzy(part, failure);
    }
  }
}

// TODO(pkalinnikov): Support SUBDOMAIN anchors.
bool IsMatch(const GURL& url,
             const UrlPattern& pattern,
             const std::vector<size_t>& failure) {
  auto subpatterns = CreateStringSplitter(pattern.url_pattern, IsWildcard);
  auto subpattern_it = subpatterns.begin();
  auto subpattern_end = subpatterns.end();

  if (subpattern_it == subpattern_end) {
    return pattern.anchor_left != proto::ANCHOR_TYPE_BOUNDARY ||
           pattern.anchor_right != proto::ANCHOR_TYPE_BOUNDARY ||
           url.is_empty();
  }

  base::StringPiece spec = url.spec();

  if (pattern.anchor_left == proto::ANCHOR_TYPE_BOUNDARY) {
    const base::StringPiece subpattern = *subpattern_it++;
    if (!StartsWithFuzzy(spec, subpattern))
      return false;
    if (subpattern_it == subpattern_end) {
      return pattern.anchor_right != proto::ANCHOR_TYPE_BOUNDARY ||
             spec.size() == subpattern.size();
    }
    spec.remove_prefix(subpattern.size());
  }

  base::StringPiece subpattern;
  auto failure_it = failure.begin();
  while (subpattern_it != subpattern_end) {
    subpattern = *subpattern_it++;
    DCHECK(!subpattern.empty());

    if (subpattern_it == subpattern_end &&
        pattern.anchor_right == proto::ANCHOR_TYPE_BOUNDARY) {
      break;
    }

    // The subpatterns with separator placeholders were indexed differently and
    // should be matched accordingly. Their failure functions were prepended by
    // a non-zero value, and we need to skip it. If the value turned to be zero,
    // it is the initial value of a failure function of a regular substring.
    CHECK(failure_it < failure.end());
    const bool has_separator_placeholders = (*failure_it != 0);
    if (has_separator_placeholders)
      ++failure_it;
    CHECK(failure_it + subpattern.size() <= failure.end());

    // If the subpattern has separator placeholders, it should be matched with
    // FindFirstOccurrenceOfSubpattern, otherwise it can be found as a regular
    // substring.
    const size_t match_end =
        (has_separator_placeholders
             ? FindFirstOccurrenceFuzzy(spec, subpattern, &*failure_it)
             : FindFirstOccurrence(spec, subpattern, &*failure_it));
    if (match_end == base::StringPiece::npos)
      return false;
    spec.remove_prefix(match_end);
    failure_it += subpattern.size();
  }

  return pattern.anchor_right != proto::ANCHOR_TYPE_BOUNDARY ||
         EndsWithFuzzy(spec, subpattern);
}

}  // namespace subresource_filter
