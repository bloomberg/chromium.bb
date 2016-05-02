// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FILTER_BUILDER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FILTER_BUILDER_H_

#include <ostream>
#include <set>
#include <vector>

#include "base/callback.h"

class ContentSettingsPattern;
class GURL;

namespace net {
class CanonicalCookie;
}

// An abstract class that builds GURL->bool predicates to filter browsing data.
// These filters can be of two modes - a whitelist or a blacklist. Different
// subclasses can have different ways of defining the set of URLs - for example,
// as domains or origins.
//
// This class defines interface to build filters for various kinds of browsing
// data. |BuildGeneralFilter()| is useful for most browsing data storage
// backends, but some backends, such as website settings and cookies, use
// other formats of filter.
class BrowsingDataFilterBuilder {
 public:
  enum Mode {
    // This means that only the origins given will be deleted.
    WHITELIST,
    // Everyone EXCEPT the origins given will be deleted.
    BLACKLIST
  };

  // Constructs a filter with the given |mode| - whitelist or blacklist.
  explicit BrowsingDataFilterBuilder(Mode mode);

  ~BrowsingDataFilterBuilder();

  // Sets the |mode| of the filter.
  void SetMode(Mode mode);

  // Returns true if we're an empty blacklist, where we delete everything.
  bool IsEmptyBlacklist() const;

  // Builds a filter that matches URLs that are in the whitelist,
  // or aren't in the blacklist.
  virtual base::Callback<bool(const GURL&)> BuildGeneralFilter() const = 0;

  // Builds a filter that matches website settings patterns that contain
  // data for URLs in the whitelist, or don't contain data for URLs in the
  // blacklist.
  virtual base::Callback<bool(const ContentSettingsPattern& pattern)>
      BuildWebsiteSettingsPatternMatchesFilter() const = 0;

  // Builds a filter that matches cookies whose sources are in the whitelist,
  // or aren't in the blacklist.
  virtual base::Callback<bool(const net::CanonicalCookie& pattern)>
      BuildCookieFilter() const = 0;

  // Builds a filter that matches channel IDs whose server identifiers are in
  // the whitelist, or aren't in the blacklist.
  virtual base::Callback<bool(const std::string& server_id)>
      BuildChannelIDFilter() const = 0;

  // A convenience method to produce an empty blacklist, a filter that matches
  // everything.
  static base::Callback<bool(const GURL&)> BuildNoopFilter();

 protected:
  Mode mode() const { return mode_; }

  // Whether or not any URLs have been added to this builder.
  virtual bool IsEmpty() const = 0;

 private:
  Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataFilterBuilder);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FILTER_BUILDER_H_
