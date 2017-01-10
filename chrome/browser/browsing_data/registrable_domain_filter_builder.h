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
//
// This filter also recognizes IP addresses and internal hostnames. Neither of
// those have subdomains (or support domain cookies), and so the filter requires
// the cookie (or other data type) source domain to be identical to the
// domain (IP address or internal hostname) for it to be considered a match.
//
// Note that e.g. "subdomain.localhost" is NOT considered to be a subdomain
// of the internal hostname "localhost". It is understood as a registrable
// domain in the scope of the TLD "localhost" (from unknown registry),
// and treated as any other registrable domain. For example,
// "www.subdomain.localhost" will be matched by a filter containing
// "subdomain.localhost". However, it is unrelated to "localhost", whose cookies
// will never be visible on "*.localhost" or vice versa.
class RegistrableDomainFilterBuilder : public BrowsingDataFilterBuilder {
 public:
  // Constructs a filter with the given |mode| - whitelist or blacklist.
  explicit RegistrableDomainFilterBuilder(Mode mode);

  ~RegistrableDomainFilterBuilder() override;

  // Adds a registerable domain to the (white- or black-) list. This is expected
  // to not include subdomains, so basically tld+1. This can also be an IP
  // address or an internal hostname.
  //
  // Refer to net/base/registry_controlled_domains/registry_controlled_domain.h
  // for more details on registrable domains and the current list of effective.
  // TLDs. We expect a string that would be returned by
  // net::registry_controlled_domains::GetDomainAndRegistry.
  void AddRegisterableDomain(const std::string& domain);

  // Builds a filter that matches URLs whose domains are in the whitelist,
  // or aren't in the blacklist.
  base::Callback<bool(const GURL&)> BuildGeneralFilter() const override;

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

  // Builds a filter that matches plugins whose |site|s are registrable domains
  // that are in the whitelist, or are not in the blacklist.
  base::Callback<bool(const std::string& site)>
      BuildPluginFilter() const override;

  // Used for testing.
  bool operator==(const RegistrableDomainFilterBuilder& other) const;

 protected:
  bool IsEmpty() const override;

 private:
  // The set of domains that constitute the whitelist or the blacklist,
  // depending on mode().
  std::set<std::string> domains_;

  DISALLOW_COPY_AND_ASSIGN(RegistrableDomainFilterBuilder);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_REGISTRABLE_DOMAIN_FILTER_BUILDER_H_
