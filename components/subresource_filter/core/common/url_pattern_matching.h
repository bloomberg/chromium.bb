// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The matching logic distinguishes between the terms URL pattern and
// subpattern. A URL pattern usually stands for the full thing, e.g.
// "example.com^*path*par=val^", whereas subpattern denotes a maximal substring
// of a pattern not containing the wildcard '*' character. For the example above
// the subpatterns are: "example.com^", "path" and "par=val^".
//
// The separator placeholder '^' symbol is used in subpatterns to match any
// separator character, which is any ASCII symbol except letters, digits, and
// the following: '_', '-', '.', '%'. Note that the separator placeholder
// character '^' is itself a separator, as well as '\0'.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_URL_PATTERN_MATCHING_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_URL_PATTERN_MATCHING_H_

#include <stddef.h>

#include <vector>

#include "base/strings/string_piece.h"
#include "url/gurl.h"

namespace subresource_filter {

struct UrlPattern;

// Builds a compound Knuth-Morris-Pratt failure function used to match URLs
// against the |pattern|.
//
// The |pattern| is split on the '*' wildcard symbol and then a failure function
// is built for each subpattern by BuildFailureFunctionFuzzy or
// BuildFailureFunction (depending on whether the subpattern contains separator
// placeholders), and appended to the returned vector. Some of the subpatterns
// can be exempted from being indexed. E.g., if the |pattern| has a BOUNDARY
// left anchor, the first subpattern can be matched by checking if it's a prefix
// of a URL.
//
// Each subpattern indexed with BuildFailureFunctionFuzzy is prepended with a
// value 1 (to distinguish them from the subpatterns indexed with
// BuildFailureFunction, their failure functions always start with 0).
//
// The URL |pattern| must be normalized. Namely, it must not have the following
// substrings: **, *<END>, <BEGIN>*. If the the |pattern| has a BOUNDARY anchor,
// the corresponding side of its string must not end with a '*' wildcard.
void BuildFailureFunction(const UrlPattern& pattern,
                          std::vector<size_t>* failure);

// Returns whether the |url| matches the URL |pattern|. The |failure| function
// must be the output of BuildFailureFunction() called with the same |pattern|.
//
// TODO(pkalinnikov): Outline algorithms implemented in this function.
bool IsMatch(const GURL& url,
             const UrlPattern& pattern,
             const std::vector<size_t>& failure);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_URL_PATTERN_MATCHING_H_
