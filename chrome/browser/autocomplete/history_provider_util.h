// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_UTIL_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_UTIL_H_
#pragma once

#include <deque>
#include <vector>

#include "chrome/browser/history/history_types.h"

namespace history {

// Used for intermediate history result operations.
struct HistoryMatch {
  // Required for STL, we don't use this directly.
  HistoryMatch();

  HistoryMatch(const URLRow& url_info,
               size_t input_location,
               bool match_in_scheme,
               bool innermost_match);

  bool operator==(const GURL& url) const;

  URLRow url_info;

  // The offset of the user's input within the URL.
  size_t input_location;

  // Whether this is a match in the scheme.  This determines whether we'll go
  // ahead and show a scheme on the URL even if the user didn't type one.
  // If our best match was in the scheme, not showing the scheme is both
  // confusing and, for inline autocomplete of the fill_into_edit, dangerous.
  // (If the user types "h" and we match "http://foo/", we need to inline
  // autocomplete that, not "foo/", which won't show anything at all, and
  // will mislead the user into thinking the What You Typed match is what's
  // selected.)
  bool match_in_scheme;

  // A match after any scheme/"www.", if the user input could match at both
  // locations.  If the user types "w", an innermost match ("website.com") is
  // better than a non-innermost match ("www.google.com").  If the user types
  // "x", no scheme in our prefix list (or "www.") begins with x, so all
  // matches are, vacuously, "innermost matches".
  bool innermost_match;
};
typedef std::deque<HistoryMatch> HistoryMatches;

struct Prefix {
  Prefix(const string16& prefix, int num_components)
      : prefix(prefix),
        num_components(num_components) {}

  string16 prefix;

  // The number of "components" in the prefix.  The scheme is a component,
  // and the initial "www." or "ftp." is a component.  So "http://foo.com"
  // and "www.bar.com" each have one component, "ftp://ftp.ftp.com" has two,
  // and "mysite.com" has none.  This is used to tell whether the user's
  // input is an innermost match or not.  See comments in HistoryMatch.
  int num_components;
};
typedef std::vector<Prefix> Prefixes;
}

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_PROVIDER_UTIL_H_
