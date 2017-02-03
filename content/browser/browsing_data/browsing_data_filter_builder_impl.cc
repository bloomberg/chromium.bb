// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "url/origin.h"

using net::registry_controlled_domains::GetDomainAndRegistry;
using net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;

namespace content {

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
// The whitelist or blacklist is represented as |origins|,
// |registerable_domains|, and |mode|.
bool MatchesURL(
    const std::set<url::Origin>& origins,
    const std::set<std::string>& registerable_domains,
    BrowsingDataFilterBuilder::Mode mode,
    const GURL& url) {
  std::string url_registerable_domain =
      GetDomainAndRegistry(url, INCLUDE_PRIVATE_REGISTRIES);
  bool found_domain =
      (registerable_domains.find(
          url_registerable_domain != "" ? url_registerable_domain
                                        : url.host()) !=
       registerable_domains.end());

  bool found_origin = (origins.find(url::Origin(url)) != origins.end());

  return ((found_domain || found_origin) ==
          (mode == BrowsingDataFilterBuilder::WHITELIST));
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

bool NoopFilter(const GURL& url) {
  return true;
}

}  // namespace

// static
std::unique_ptr<BrowsingDataFilterBuilder>
BrowsingDataFilterBuilder::Create(Mode mode) {
  return base::MakeUnique<BrowsingDataFilterBuilderImpl>(mode);
}

// static
base::Callback<bool(const GURL&)> BrowsingDataFilterBuilder::BuildNoopFilter() {
  return base::Bind(&NoopFilter);
}

BrowsingDataFilterBuilderImpl::BrowsingDataFilterBuilderImpl(Mode mode)
    : mode_(mode) {}

BrowsingDataFilterBuilderImpl::~BrowsingDataFilterBuilderImpl() {}

void BrowsingDataFilterBuilderImpl::AddOrigin(const url::Origin& origin) {
  // TODO(msramek): Optimize OriginFilterBuilder for larger filters if needed.
  DCHECK_LE(origins_.size(), 10U) << "OriginFilterBuilder is only suitable "
                                     "for creating small filters.";

  // By limiting the filter to non-unique origins, we can guarantee that
  // origin1 < origin2 && origin1 > origin2 <=> origin1.isSameOrigin(origin2).
  // This means that std::set::find() will use the same semantics for
  // origin comparison as Origin::IsSameOriginWith(). Furthermore, this
  // means that two filters are equal iff they are equal element-wise.
  DCHECK(!origin.unique()) << "Invalid origin passed into OriginFilter.";

  // TODO(msramek): All urls with file scheme currently map to the same
  // origin. This is currently not a problem, but if it becomes one,
  // consider recognizing the URL path.

  origins_.insert(origin);
}

void BrowsingDataFilterBuilderImpl::AddRegisterableDomain(
    const std::string& domain) {
  // We check that the domain we're given is actually a eTLD+1, an IP address,
  // or an internal hostname.
  DCHECK(!IsSubdomainOfARegistrableDomain(domain));
  domains_.insert(domain);
}

bool BrowsingDataFilterBuilderImpl::IsEmptyBlacklist() const {
  return mode_ == Mode::BLACKLIST && origins_.empty() && domains_.empty();
}

base::RepeatingCallback<bool(const GURL&)>
BrowsingDataFilterBuilderImpl::BuildGeneralFilter() const {
  return base::BindRepeating(&MatchesURL, origins_, domains_, mode_);
}

base::RepeatingCallback<bool(const net::CanonicalCookie& cookie)>
BrowsingDataFilterBuilderImpl::BuildCookieFilter() const {
  DCHECK(origins_.empty()) <<
      "Origin-based deletion is not suitable for cookies. Please use "
      "different scoping, such as RegistrableDomainFilterBuilder.";
  return base::BindRepeating(&MatchesCookieForRegisterableDomainsAndIPs,
                             domains_, mode_);
}

base::RepeatingCallback<bool(const std::string& channel_id_server_id)>
BrowsingDataFilterBuilderImpl::BuildChannelIDFilter() const {
  DCHECK(origins_.empty()) <<
      "Origin-based deletion is not suitable for channel IDs. Please use "
      "different scoping, such as RegistrableDomainFilterBuilder.";
  return base::BindRepeating(&MatchesChannelIDForRegisterableDomainsAndIPs,
                             domains_, mode_);
}

base::RepeatingCallback<bool(const std::string& site)>
BrowsingDataFilterBuilderImpl::BuildPluginFilter() const {
  DCHECK(origins_.empty()) <<
      "Origin-based deletion is not suitable for plugins. Please use "
      "different scoping, such as RegistrableDomainFilterBuilder.";
  return base::BindRepeating(&MatchesPluginSiteForRegisterableDomainsAndIPs,
                             domains_, mode_);
}

BrowsingDataFilterBuilderImpl::Mode
BrowsingDataFilterBuilderImpl::GetMode() const {
  return mode_;
}

std::unique_ptr<BrowsingDataFilterBuilder>
BrowsingDataFilterBuilderImpl::Copy() const {
  std::unique_ptr<BrowsingDataFilterBuilderImpl> copy =
      base::MakeUnique<BrowsingDataFilterBuilderImpl>(mode_);
  copy->origins_ = origins_;
  copy->domains_ = domains_;
  return std::move(copy);
}

bool BrowsingDataFilterBuilderImpl::operator==(
    const BrowsingDataFilterBuilder& other) const {
  // This is the only implementation of BrowsingDataFilterBuilder, so we can
  // downcast |other|.
  const BrowsingDataFilterBuilderImpl* other_impl =
      static_cast<const BrowsingDataFilterBuilderImpl*>(&other);

  return origins_ == other_impl->origins_ &&
         domains_ == other_impl->domains_ &&
         mode_ == other_impl->mode_;
}

}  // namespace content
