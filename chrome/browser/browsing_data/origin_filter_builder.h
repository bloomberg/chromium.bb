// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_ORIGIN_FILTER_BUILDER_H_
#define CHROME_BROWSER_BROWSING_DATA_ORIGIN_FILTER_BUILDER_H_

#include <ostream>
#include <set>
#include <vector>

#include "base/callback.h"
#include "url/gurl.h"
#include "url/origin.h"

// A class that constructs URL deletion filters (represented as GURL->bool
// predicates) that match certain origins.
class OriginFilterBuilder {
 public:
  enum Mode {
    WHITELIST,
    BLACKLIST
  };

  // Constructs a filter with the given |mode| - whitelist or blacklist.
  explicit OriginFilterBuilder(Mode mode);

  ~OriginFilterBuilder();

  // Adds the |origin| to the (white- or black-) list.
  void AddOrigin(const url::Origin& origin);

  // Sets the |mode| of the filter.
  void SetMode(Mode mode);

  // Builds a filter that matches URLs whose origins are in the whitelist,
  // or aren't in the blacklist.
  base::Callback<bool(const GURL&)> BuildSameOriginFilter() const;

  // Build a filter that matches URLs whose origins, or origins obtained by
  // replacing the host with any superdomain, are listed in the whitelist,
  // or are not listed in the blacklist.
  base::Callback<bool(const GURL&)> BuildDomainFilter() const;

  // A convenience method to produce an empty blacklist, a filter that matches
  // everything.
  static base::Callback<bool(const GURL&)> BuildNoopFilter();

 private:
  // True if the origin of |url| is in the whitelist, or isn't in the blacklist.
  // The whitelist or blacklist is represented as |origins| and |mode|.
  static bool MatchesURL(
      std::set<url::Origin>* origins, Mode mode, const GURL& url);

  // True if any origin [scheme, host, port], such that |url| has the same
  // scheme and port, and |url|'s host is the same or a subdomain of that host,
  // is in the whitelist, or isn't in the blacklist. The whitelist or blacklist
  // is represented as |origins| and |mode|.
  static bool MatchesURLWithSubdomains(
      std::set<url::Origin>* origins, Mode mode, const GURL& url);

  // The list of origins and whether they should be interpreted as a whitelist
  // or blacklist.
  std::set<url::Origin> origin_list_;
  Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(OriginFilterBuilder);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_ORIGIN_FILTER_BUILDER_H_
