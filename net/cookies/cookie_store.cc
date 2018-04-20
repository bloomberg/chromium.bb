// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_store.h"

#include "base/bind.h"
#include "base/callback.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_options.h"

namespace net {

namespace {

// Return true if the eTLD+1 of the cookies domain matches any of the strings
// in |match_domains|, false otherwise.
bool DomainMatchesDomainSet(const net::CanonicalCookie& cookie,
                            const std::set<std::string>& match_domains) {
  if (match_domains.empty())
    return false;

  // If domain is an IP address it returns an empty string.
  std::string effective_domain(
      net::registry_controlled_domains::GetDomainAndRegistry(
          // GetDomainAndRegistry() is insensitive to leading dots, i.e.
          // to host/domain cookie distinctions.
          cookie.Domain(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  // If the cookie's domain is is not parsed as belonging to a registry
  // (e.g. for IP addresses or internal hostnames) an empty string will be
  // returned.  In this case, use the domain in the cookie.
  if (effective_domain.empty()) {
    if (cookie.IsDomainCookie())
      effective_domain = cookie.Domain().substr(1);
    else
      effective_domain = cookie.Domain();
  }

  return match_domains.count(effective_domain) != 0;
}

}  // anonymous namespace

CookieStore::~CookieStore() = default;

void CookieStore::DeleteAllAsync(DeleteCallback callback) {
  DeleteAllCreatedInTimeRangeAsync(TimeRange(), std::move(callback));
}

void CookieStore::SetForceKeepSessionState() {
  // By default, do nothing.
}

void CookieStore::GetAllCookiesForURLAsync(const GURL& url,
                                           GetCookieListCallback callback) {
  CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_mode(
      CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  options.set_do_not_update_access_time();
  GetCookieListWithOptionsAsync(url, options, std::move(callback));
}

void CookieStore::SetChannelIDServiceID(int id) {
  DCHECK_EQ(-1, channel_id_service_id_);
  channel_id_service_id_ = id;
}

int CookieStore::GetChannelIDServiceID() {
  return channel_id_service_id_;
}

void CookieStore::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_absolute_name) const {}

CookieStore::CookieStore() : channel_id_service_id_(-1) {}

CookieStore::TimeRange::TimeRange() = default;

CookieStore::TimeRange::TimeRange(const TimeRange& other) = default;

CookieStore::TimeRange::TimeRange(base::Time start, base::Time end)
    : start_(start), end_(end) {
  if (!start.is_null() && !end.is_null())
    DCHECK_GE(end, start);
}

CookieStore::TimeRange& CookieStore::TimeRange::operator=(
    const TimeRange& rhs) = default;

bool CookieStore::TimeRange::Contains(const base::Time& time) const {
  DCHECK(!time.is_null());

  if (!start_.is_null() && start_ == end_)
    return time == start_;
  return (start_.is_null() || start_ <= time) &&
         (end_.is_null() || time < end_);
}

void CookieStore::TimeRange::SetStart(base::Time value) {
  start_ = value;
}

void CookieStore::TimeRange::SetEnd(base::Time value) {
  end_ = value;
}

CookieStore::CookieDeletionInfo::CookieDeletionInfo() = default;

CookieStore::CookieDeletionInfo::CookieDeletionInfo(base::Time start_time,
                                                    base::Time end_time)
    : creation_range(start_time, end_time) {}

CookieStore::CookieDeletionInfo::CookieDeletionInfo(
    CookieDeletionInfo&& other) = default;

CookieStore::CookieDeletionInfo::CookieDeletionInfo(
    const CookieDeletionInfo& other) = default;

CookieStore::CookieDeletionInfo::~CookieDeletionInfo() = default;

CookieStore::CookieDeletionInfo& CookieStore::CookieDeletionInfo::operator=(
    CookieDeletionInfo&& rhs) = default;

CookieStore::CookieDeletionInfo& CookieStore::CookieDeletionInfo::operator=(
    const CookieDeletionInfo& rhs) = default;

bool CookieStore::CookieDeletionInfo::Matches(
    const CanonicalCookie& cookie) const {
  if (session_control != SessionControl::IGNORE_CONTROL &&
      (cookie.IsPersistent() !=
       (session_control == SessionControl::PERSISTENT_COOKIES))) {
    return false;
  }

  if (!creation_range.Contains(cookie.CreationDate()))
    return false;

  if (host.has_value() &&
      !(cookie.IsHostCookie() && cookie.IsDomainMatch(host.value()))) {
    return false;
  }

  if (name.has_value() && cookie.Name() != name)
    return false;

  if (value_for_testing.has_value() &&
      value_for_testing.value() != cookie.Value()) {
    return false;
  }

  if (url.has_value() &&
      !cookie.IncludeForRequestURL(url.value(), cookie_options)) {
    return false;
  }

  if (!domains_and_ips_to_delete.empty() &&
      !DomainMatchesDomainSet(cookie, domains_and_ips_to_delete)) {
    return false;
  }

  if (!domains_and_ips_to_ignore.empty() &&
      DomainMatchesDomainSet(cookie, domains_and_ips_to_ignore)) {
    return false;
  }

  return true;
}

}  // namespace net
