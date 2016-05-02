// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_REGISTRABLE_DOMAIN_FILTER_BUILDER_H_
#define CHROME_BROWSER_BROWSING_DATA_REGISTRABLE_DOMAIN_FILTER_BUILDER_H_

#include <ostream>
#include <set>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/browsing_data/browsing_data_filter_builder.h"
#include "url/gurl.h"

// A class that constructs URL deletion filters (represented as GURL->bool
// predicates) that match registerable domains - which is basically an eTLD + 1.
// This means that we ignore schemes and subdomains. This is particularly
// suitable for cookie filtering because of the cookie visibility model.
//
// Cookies are domain-scoped, and websites often rely on cookies that are living
// on various subdomains. For example, plus.google.com relies on google.com
// cookies, which eventually talks to account.google.com cookies for GAIA
// account auth. This means that when we save cookies for an origin, we need
// to save all cookies for the eTLD+1. This means blacklisting (or whitelisting)
// https://plus.google.com will have us save (or delete) any cookies for
// *.google.com (http://www.google.com, https://accounts.google.com, etc). For
// this reason we don't use origins, and instead use registerable domains.
//
// See net/base/registry_controlled_domains/registry_controlled_domain.h for
// more details on registrable domains and the current list of effective eTLDs.
class RegistrableDomainFilterBuilder : public BrowsingDataFilterBuilder {
 public:
  // Constructs a filter with the given |mode| - whitelist or blacklist.
  explicit RegistrableDomainFilterBuilder(Mode mode);

  ~RegistrableDomainFilterBuilder();

  // Adds a registerable domain to the (white- or black-) list. This is expected
  // to not include subdomains, so basically tld+1. This can also be an IP
  // address.
  // Refer to net/base/registry_controlled_domains/registry_controlled_domain.h
  // for more details on registrable domains and the current list of effective.
  // TLDs. We expect a string that would be returned by
  // net::registry_controlled_domains::GetDomainAndRegistry.
  void AddRegisterableDomain(const std::string& domain);

  // Builds a filter that matches URLs whose domains are in the whitelist,
  // or aren't in the blacklist.
  base::Callback<bool(const GURL&)> BuildGeneralFilter() const override;

  // Builds a filter that calls ContentSettingsPattern::Compare on the given
  // pattern and a new pattern constructed by each domain in this filter. The
  // domain pattern A and given pattern B match when A.Compare(B) is IDENTITY
  // or PREDECESSOR. This means we only match patterns that are the same pattern
  // or a more specific pattern than our domain (so we shouldn't be matching
  // wildcard patterns like "*" or "*:80").
  base::Callback<bool(const ContentSettingsPattern& pattern)>
      BuildWebsiteSettingsPatternMatchesFilter() const override;

  // We do a direct comparison to the registerable domain of the cookie. A
  // whitelist filter will return true if any of its domains match the cookie,
  // and a blacklist filter will return true only if none of its domains match
  // the cookie.
  base::Callback<bool(const net::CanonicalCookie& cookie)>
      BuildCookieFilter() const override;

  // Builds a filter that matches channel IDs whose server identifiers are
  // identical to one of the registrable domains that are in the whitelist,
  // or are not in the blacklist.
  base::Callback<bool(const std::string& cookie)>
      BuildChannelIDFilter() const override;

 protected:
  bool IsEmpty() const override;

 private:
  // True if the domain of |url| is in the whitelist, or isn't in the blacklist.
  // The whitelist or blacklist is represented as |registerable_domains|
  // and |mode|.
  static bool MatchesURL(std::set<std::string>* registerable_domains,
                         Mode mode,
                         const GURL& url);

  // True if the pattern something in the whitelist, or doesn't match something
  // in the blacklist.
  // The whitelist or blacklist is represented as |patterns|,  and |mode|.
  static bool MatchesWebsiteSettingsPattern(
      std::vector<ContentSettingsPattern>* patterns,
      Mode mode,
      const ContentSettingsPattern& pattern);

  // True if no domains can see the given cookie and we're a blacklist, or any
  // domains can see the cookie and we're a whitelist.
  // The whitelist or blacklist is represented as |domains_and_ips| and |mode|.
  static bool MatchesCookieForRegisterableDomainsAndIPs(
      std::set<std::string>* domains_and_ips,
      Mode mode,
      const net::CanonicalCookie& cookie);

  // True if none of the supplied domains matches this Channel ID's server ID
  // and we're a blacklist, or one of them does and we're a whitelist.
  // The whitelist or blacklist is represented as |domains_and_ips| and |mode|.
  static bool MatchesChannelIDForRegisterableDomainsAndIPs(
      std::set<std::string>* domains_and_ips,
      Mode mode,
      const std::string& channel_id_server_id);

  // The list of domains and whether they should be interpreted as a whitelist
  // or blacklist.
  std::set<std::string> domain_list_;

  DISALLOW_COPY_AND_ASSIGN(RegistrableDomainFilterBuilder);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_REGISTRABLE_DOMAIN_FILTER_BUILDER_H_
