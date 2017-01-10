// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/registrable_domain_filter_builder.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"

using net::registry_controlled_domains::GetDomainAndRegistry;
using net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;

namespace {

// Whether this is a subdomain of a registrable domain.
bool IsSubdomainOfARegistrableDomain(const std::string& domain) {
  std::string registrable_domain =
      GetDomainAndRegistry(domain, INCLUDE_PRIVATE_REGISTRIES);
  return registrable_domain != domain && registrable_domain != "";
}

// Note that for every domain, exactly one of the following holds:
// 1. GetDomainAndRegistry(domain, _) == ""        - e.g. localhost, 127.0.0.1
// 2. GetDomainAndRegistry(domain, _) == domain    - e.g. google.com
// 3. IsSubdomainOfARegistrableDomain(domain)      - e.g. www.google.com
// Types 1 and 2 are supported by RegistrableDomainFilterBuilder. Type 3 is not.


// True if the domain of |url| is in the whitelist, or isn't in the blacklist.
// The whitelist or blacklist is represented as |registerable_domains|
// and |mode|.
bool MatchesURL(
    const std::set<std::string>& registerable_domains,
    BrowsingDataFilterBuilder::Mode mode,
    const GURL& url) {
  std::string url_registerable_domain =
      GetDomainAndRegistry(url, INCLUDE_PRIVATE_REGISTRIES);
  return (registerable_domains.find(
              url_registerable_domain != "" ? url_registerable_domain
                                            : url.host()) !=
          registerable_domains.end()) ==
         (mode == BrowsingDataFilterBuilder::WHITELIST);
}

// True if no domains can see the given cookie and we're a blacklist, or any
// domains can see the cookie and we're a whitelist.
// The whitelist or blacklist is represented as |domains_and_ips| and |mode|.
bool MatchesCookieForRegisterableDomainsAndIPs(
    const std::set<std::string>& domains_and_ips,
    BrowsingDataFilterBuilder::Mode mode,
    const net::CanonicalCookie& cookie) {
  if (domains_and_ips.empty())
    return mode == BrowsingDataFilterBuilder::BLACKLIST;
  std::string cookie_domain = cookie.Domain();
  if (cookie.IsDomainCookie())
    cookie_domain = cookie_domain.substr(1);
  std::string parsed_cookie_domain =
      GetDomainAndRegistry(cookie_domain, INCLUDE_PRIVATE_REGISTRIES);
  // This means we're an IP address or an internal hostname.
  if (parsed_cookie_domain.empty())
    parsed_cookie_domain = cookie_domain;
  return (mode == BrowsingDataFilterBuilder::WHITELIST) ==
      (domains_and_ips.find(parsed_cookie_domain) != domains_and_ips.end());
}

// True if none of the supplied domains matches this Channel ID's server ID
// and we're a blacklist, or one of them does and we're a whitelist.
// The whitelist or blacklist is represented as |domains_and_ips| and |mode|.
bool MatchesChannelIDForRegisterableDomainsAndIPs(
    const std::set<std::string>& domains_and_ips,
    BrowsingDataFilterBuilder::Mode mode,
    const std::string& channel_id_server_id) {
  return ((mode == BrowsingDataFilterBuilder::WHITELIST) ==
      (domains_and_ips.find(channel_id_server_id) != domains_and_ips.end()));
}

// True if none of the supplied domains matches this plugin's |site| and we're
// a blacklist, or one of them does and we're a whitelist. The whitelist or
// blacklist is represented by |domains_and_ips| and |mode|.
bool MatchesPluginSiteForRegisterableDomainsAndIPs(
    const std::set<std::string>& domains_and_ips,
    BrowsingDataFilterBuilder::Mode mode,
    const std::string& site) {
  // If |site| is a third- or lower-level domain, find the corresponding eTLD+1.
  std::string domain_or_ip =
      GetDomainAndRegistry(site, INCLUDE_PRIVATE_REGISTRIES);
  if (domain_or_ip.empty())
    domain_or_ip = site;

  return ((mode == BrowsingDataFilterBuilder::WHITELIST) ==
      (domains_and_ips.find(domain_or_ip) != domains_and_ips.end()));
}

}  // namespace

RegistrableDomainFilterBuilder::RegistrableDomainFilterBuilder(Mode mode)
    : BrowsingDataFilterBuilder(mode) {
}

RegistrableDomainFilterBuilder::~RegistrableDomainFilterBuilder() {}

void RegistrableDomainFilterBuilder::AddRegisterableDomain(
    const std::string& domain) {
  // We check that the domain we're given is actually a eTLD+1, an IP address,
  // or an internal hostname.
  DCHECK(!IsSubdomainOfARegistrableDomain(domain));
  domains_.insert(domain);
}

base::Callback<bool(const GURL&)>
RegistrableDomainFilterBuilder::BuildGeneralFilter() const {
  return base::BindRepeating(MatchesURL, domains_, mode());
}

base::Callback<bool(const net::CanonicalCookie& cookie)>
RegistrableDomainFilterBuilder::BuildCookieFilter() const {
  return base::BindRepeating(&MatchesCookieForRegisterableDomainsAndIPs,
                             domains_, mode());
}

base::Callback<bool(const std::string& cookie)>
RegistrableDomainFilterBuilder::BuildChannelIDFilter() const {
  return base::BindRepeating(&MatchesChannelIDForRegisterableDomainsAndIPs,
                             domains_, mode());
}

base::Callback<bool(const std::string& site)>
RegistrableDomainFilterBuilder::BuildPluginFilter() const {
  return base::BindRepeating(&MatchesPluginSiteForRegisterableDomainsAndIPs,
                             domains_, mode());
}

bool RegistrableDomainFilterBuilder::operator==(
    const RegistrableDomainFilterBuilder& other) const {
  return domains_ == other.domains_ && mode() == other.mode();
}

bool RegistrableDomainFilterBuilder::IsEmpty() const {
  return domains_.empty();
}
