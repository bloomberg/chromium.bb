// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_ORIGIN_FILTER_BUILDER_H_
#define CHROME_BROWSER_BROWSING_DATA_ORIGIN_FILTER_BUILDER_H_

#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/browsing_data/browsing_data_filter_builder.h"
#include "url/gurl.h"
#include "url/origin.h"

// A class that constructs URL deletion filters (represented as GURL->bool
// predicates) that match certain origins.
//
// IMPORTANT NOTE: While this class does define cookie and channel IDs filtering
// methods, as required by the BrowsingDataFilterBuilder interface, it is not
// suitable for deletion of those data types, as they are scoped to eTLD+1.
// Instead, use RegistrableDomainFilterBuilder and see its documenation for
// more details.
class OriginFilterBuilder : public BrowsingDataFilterBuilder {
 public:
  // Constructs a filter with the given |mode| - whitelist or blacklist.
  explicit OriginFilterBuilder(Mode mode);

  ~OriginFilterBuilder();

  // Adds the |origin| to the (white- or black-) list.
  void AddOrigin(const url::Origin& origin);

  // Builds a filter that matches URLs whose origins are in the whitelist,
  // or aren't in the blacklist.
  base::Callback<bool(const GURL&)> BuildGeneralFilter() const override;

  // Builds a filter that calls ContentSettingsPattern::Compare on the given
  // pattern and a new pattern constructed by each origin in this filter. The
  // domain pattern A and given pattern B match when A.Compare(B) is IDENTITY.
  // This is sufficient, because website settings should only ever store
  // origin-scoped patterns. Furthermore, website settings patterns support
  // paths, but only for the file:// scheme, which is in turn not supported
  // by OriginFilterBuilder.
  base::Callback<bool(const ContentSettingsPattern& pattern)>
      BuildWebsiteSettingsPatternMatchesFilter() const override;

  // Cookie filter is not implemented in this subclass. Please use
  // a BrowsingDataFilterBuilder with different scoping,
  // such as RegistrableDomainFilterBuilder.
  base::Callback<bool(const net::CanonicalCookie& cookie)>
      BuildCookieFilter() const override;

  // Channel ID filter is not implemented in this subclasss. Please use
  // a BrowsingDataFilterBuilder with different scoping,
  // such as RegistrableDomainFilterBuilder.
  base::Callback<bool(const std::string& channel_id_server_id)>
      BuildChannelIDFilter() const override;

 protected:
  bool IsEmpty() const override;

 private:
  // True if the origin of |url| is in the whitelist, or isn't in the blacklist.
  // The whitelist or blacklist is represented as |origins| and |mode|.
  static bool MatchesURL(
      std::set<url::Origin>* origins, Mode mode, const GURL& url);

  // True if the |pattern| is identical to one of the |origin_patterns|.
  static bool MatchesWebsiteSettingsPattern(
      std::vector<ContentSettingsPattern>* origin_patterns,
      Mode mode,
      const ContentSettingsPattern& pattern);

  // The list of origins and whether they should be interpreted as a whitelist
  // or blacklist.
  std::set<url::Origin> origin_list_;
  Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(OriginFilterBuilder);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_ORIGIN_FILTER_BUILDER_H_
