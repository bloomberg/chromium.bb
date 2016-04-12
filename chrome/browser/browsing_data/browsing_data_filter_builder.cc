// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/browsing_data/browsing_data_filter_builder.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using net::registry_controlled_domains::GetDomainAndRegistry;
using net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;
using Relation = ContentSettingsPattern::Relation;

namespace {

bool NoopFilter(const GURL& url) {
  return true;
}

}  // namespace

BrowsingDataFilterBuilder::BrowsingDataFilterBuilder(Mode mode) : mode_(mode) {}

BrowsingDataFilterBuilder::~BrowsingDataFilterBuilder() {}

void BrowsingDataFilterBuilder::AddRegisterableDomain(
    const std::string& domain) {
  // We check that the domain we're given is actually a eTLD+1, or an IP
  // address.
  DCHECK(GetDomainAndRegistry("www." + domain, INCLUDE_PRIVATE_REGISTRIES) ==
             domain ||
         GURL("http://" + domain).HostIsIPAddress());
  domain_list_.insert(domain);
}

void BrowsingDataFilterBuilder::SetMode(Mode mode) {
  mode_ = mode;
}

bool BrowsingDataFilterBuilder::IsEmptyBlacklist() const {
  return mode_ == Mode::BLACKLIST && domain_list_.empty();
}

base::Callback<bool(const GURL&)>
BrowsingDataFilterBuilder::BuildSameDomainFilter() const {
  std::set<std::string>* domains = new std::set<std::string>(domain_list_);
  return base::Bind(&BrowsingDataFilterBuilder::MatchesURL,
                    base::Owned(domains), mode_);
}

base::Callback<bool(const ContentSettingsPattern& pattern)>
BrowsingDataFilterBuilder::BuildWebsiteSettingsPatternMatchesFilter() const {
  std::vector<ContentSettingsPattern>* patterns_from_domains =
      new std::vector<ContentSettingsPattern>();
  patterns_from_domains->reserve(domain_list_.size());

  std::unique_ptr<ContentSettingsPattern::BuilderInterface> builder(
      ContentSettingsPattern::CreateBuilder(/* use_legacy_validate */ false));
  for (const std::string& domain : domain_list_) {
    builder->WithSchemeWildcard()
        ->WithPortWildcard()
        ->WithPathWildcard()
        ->WithHost(domain);
    if (!GURL("http://" + domain).HostIsIPAddress()) {
      builder->WithDomainWildcard();
    }
    patterns_from_domains->push_back(builder->Build());
  }

  for (const ContentSettingsPattern& domain : *patterns_from_domains) {
    DCHECK(domain.IsValid());
  }

  return base::Bind(&BrowsingDataFilterBuilder::MatchesWebsiteSettingsPattern,
                    base::Owned(patterns_from_domains), mode_);
}

base::Callback<bool(const net::CanonicalCookie& pattern)>
BrowsingDataFilterBuilder::BuildDomainCookieFilter() const {
  std::set<std::string>* domains_and_ips =
      new std::set<std::string>(domain_list_);
  return base::Bind(
      &BrowsingDataFilterBuilder::MatchesCookieForRegisterableDomainsAndIPs,
      base::Owned(domains_and_ips), mode_);
}

// static
base::Callback<bool(const GURL&)> BrowsingDataFilterBuilder::BuildNoopFilter() {
  return base::Bind(&NoopFilter);
}

// static
bool BrowsingDataFilterBuilder::MatchesURL(
    std::set<std::string>* registerable_domains,
    Mode mode,
    const GURL& url) {
  std::string url_registerable_domain =
      GetDomainAndRegistry(url, INCLUDE_PRIVATE_REGISTRIES);
  return (registerable_domains->find(url_registerable_domain) !=
              registerable_domains->end() ||
          (url.HostIsIPAddress() && (registerable_domains->find(url.host()) !=
                                     registerable_domains->end()))) ==
         (mode == WHITELIST);
}

// static
bool BrowsingDataFilterBuilder::MatchesWebsiteSettingsPattern(
    std::vector<ContentSettingsPattern>* domain_patterns,
    Mode mode,
    const ContentSettingsPattern& pattern) {
  for (const ContentSettingsPattern& domain : *domain_patterns) {
    DCHECK(domain.IsValid());
    Relation relation = pattern.Compare(domain);
    if (relation == Relation::IDENTITY || relation == Relation::PREDECESSOR)
      return mode == WHITELIST;
  }
  return mode != WHITELIST;
}

// static
bool BrowsingDataFilterBuilder::MatchesCookieForRegisterableDomainsAndIPs(
    std::set<std::string>* domains_and_ips,
    Mode mode,
    const net::CanonicalCookie& cookie) {
  if (domains_and_ips->empty())
    return mode == BLACKLIST;
  std::string cookie_domain = cookie.Domain();
  if (cookie.IsDomainCookie())
    cookie_domain = cookie_domain.substr(1);
  std::string parsed_cookie_domain =
      GetDomainAndRegistry(cookie_domain, INCLUDE_PRIVATE_REGISTRIES);
  // This means we're an IP address.
  if (parsed_cookie_domain.empty())
    parsed_cookie_domain = cookie_domain;
  return (mode == WHITELIST) == (domains_and_ips->find(parsed_cookie_domain) !=
                                 domains_and_ips->end());
}
