// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this code based on Mozilla:
//   (netwerk/cookie/src/nsCookieService.cpp)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/base/cookie_monster.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/scoped_ptr.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

static const int kMinutesInTenYears = 10 * 365 * 24 * 60;
static const int kMonsterAlive = 0xBBEEEEFF;
static const int kMonsterDead = 0xFFEEEEDD;

namespace net {

// See comments at declaration of these variables in cookie_monster.h
// for details.
const size_t CookieMonster::kDomainMaxCookies           = 180;
const size_t CookieMonster::kDomainPurgeCookies         = 30;
const size_t CookieMonster::kMaxCookies                 = 3300;
const size_t CookieMonster::kPurgeCookies               = 300;
const int CookieMonster::kSafeFromGlobalPurgeDays       = 30;

namespace {

// Default minimum delay after updating a cookie's LastAccessDate before we
// will update it again.
const int kDefaultAccessUpdateThresholdSeconds = 60;

// Comparator to sort cookies from highest creation date to lowest
// creation date.
struct OrderByCreationTimeDesc {
  bool operator()(const CookieMonster::CookieMap::iterator& a,
                  const CookieMonster::CookieMap::iterator& b) const {
    return a->second->CreationDate() > b->second->CreationDate();
  }
};

// Constants for use in VLOG
const int kVlogPerCookieMonster = 1;
const int kVlogPeriodic = 3;
const int kVlogGarbageCollection = 5;
const int kVlogSetCookies = 7;
const int kVlogGetCookies = 9;

// Mozilla sorts on the path length (longest first), and then it
// sorts by creation time (oldest first).
// The RFC says the sort order for the domain attribute is undefined.
bool CookieSorter(CookieMonster::CanonicalCookie* cc1,
                  CookieMonster::CanonicalCookie* cc2) {
  if (cc1->Path().length() == cc2->Path().length())
    return cc1->CreationDate() < cc2->CreationDate();
  return cc1->Path().length() > cc2->Path().length();
}

bool LRUCookieSorter(const CookieMonster::CookieMap::iterator& it1,
                     const CookieMonster::CookieMap::iterator& it2) {
  // Cookies accessed less recently should be deleted first.
  if (it1->second->LastAccessDate() != it2->second->LastAccessDate())
    return it1->second->LastAccessDate() < it2->second->LastAccessDate();

  // In rare cases we might have two cookies with identical last access times.
  // To preserve the stability of the sort, in these cases prefer to delete
  // older cookies over newer ones.  CreationDate() is guaranteed to be unique.
  return it1->second->CreationDate() < it2->second->CreationDate();
}

// Our strategy to find duplicates is:
// (1) Build a map from (cookiename, cookiepath) to
//     {list of cookies with this signature, sorted by creation time}.
// (2) For each list with more than 1 entry, keep the cookie having the
//     most recent creation time, and delete the others.
//
// Two cookies are considered equivalent if they have the same domain,
// name, and path.
struct CookieSignature {
 public:
  CookieSignature(const std::string& name, const std::string& domain,
                  const std::string& path)
      : name(name),
        domain(domain),
        path(path) {}

  // To be a key for a map this class needs to be assignable, copyable,
  // and have an operator<.  The default assignment operator
  // and copy constructor are exactly what we want.

  bool operator<(const CookieSignature& cs) const {
    // Name compare dominates, then domain, then path.
    int diff = name.compare(cs.name);
    if (diff != 0)
      return diff < 0;

    diff = domain.compare(cs.domain);
    if (diff != 0)
      return diff < 0;

    return path.compare(cs.path) < 0;
  }

  std::string name;
  std::string domain;
  std::string path;
};

// Returns the effective TLD+1 for a given host. This only makes sense for http
// and https schemes. For other schemes, the host will be returned unchanged
// (minus any leading period).
std::string GetEffectiveDomain(const std::string& scheme,
                               const std::string& host) {
  if (scheme == "http" || scheme == "https")
    return RegistryControlledDomainService::GetDomainAndRegistry(host);

  if (!CookieMonster::DomainIsHostOnly(host))
    return host.substr(1);
  return host;
}

// Determine the actual cookie domain based on the domain string passed
// (if any) and the URL from which the cookie came.
// On success returns true, and sets cookie_domain to either a
//   -host cookie domain (ex: "google.com")
//   -domain cookie domain (ex: ".google.com")
bool GetCookieDomainWithString(const GURL& url,
                               const std::string& domain_string,
                               std::string* result) {
  const std::string url_host(url.host());

  // If no domain was specified in the domain string, default to a host cookie.
  // We match IE/Firefox in allowing a domain=IPADDR if it matches the url
  // ip address hostname exactly.  It should be treated as a host cookie.
  if (domain_string.empty() ||
      (url.HostIsIPAddress() && url_host == domain_string)) {
    *result = url_host;
    DCHECK(CookieMonster::DomainIsHostOnly(*result));
    return true;
  }

  // Get the normalized domain specified in cookie line.
  // Note: The RFC says we can reject a cookie if the domain
  // attribute does not start with a dot. IE/FF/Safari however, allow a cookie
  // of the form domain=my.domain.com, treating it the same as
  // domain=.my.domain.com -- for compatibility we do the same here.  Firefox
  // also treats domain=.....my.domain.com like domain=.my.domain.com, but
  // neither IE nor Safari do this, and we don't either.
  url_canon::CanonHostInfo ignored;
  std::string cookie_domain(net::CanonicalizeHost(domain_string, &ignored));
  if (cookie_domain.empty())
    return false;
  if (cookie_domain[0] != '.')
    cookie_domain = "." + cookie_domain;

  // Ensure |url| and |cookie_domain| have the same domain+registry.
  const std::string url_scheme(url.scheme());
  const std::string url_domain_and_registry(
      GetEffectiveDomain(url_scheme, url_host));
  if (url_domain_and_registry.empty())
    return false;  // IP addresses/intranet hosts can't set domain cookies.
  const std::string cookie_domain_and_registry(
      GetEffectiveDomain(url_scheme, cookie_domain));
  if (url_domain_and_registry != cookie_domain_and_registry)
    return false;  // Can't set a cookie on a different domain + registry.

  // Ensure |url_host| is |cookie_domain| or one of its subdomains.  Given that
  // we know the domain+registry are the same from the above checks, this is
  // basically a simple string suffix check.
  if ((url_host.length() < cookie_domain.length()) ?
      (cookie_domain != ("." + url_host)) :
      url_host.compare(url_host.length() - cookie_domain.length(),
                       cookie_domain.length(), cookie_domain))
    return false;

  *result = cookie_domain;
  return true;
}

// Determine the cookie domain to use for setting the specified cookie.
bool GetCookieDomain(const GURL& url,
                     const CookieMonster::ParsedCookie& pc,
                     std::string* result) {
  std::string domain_string;
  if (pc.HasDomain())
    domain_string = pc.Domain();
  return GetCookieDomainWithString(url, domain_string, result);
}

std::string CanonPathWithString(const GURL& url,
                                const std::string& path_string) {
  // The RFC says the path should be a prefix of the current URL path.
  // However, Mozilla allows you to set any path for compatibility with
  // broken websites.  We unfortunately will mimic this behavior.  We try
  // to be generous and accept cookies with an invalid path attribute, and
  // default the path to something reasonable.

  // The path was supplied in the cookie, we'll take it.
  if (!path_string.empty() && path_string[0] == '/')
    return path_string;

  // The path was not supplied in the cookie or invalid, we will default
  // to the current URL path.
  // """Defaults to the path of the request URL that generated the
  //    Set-Cookie response, up to, but not including, the
  //    right-most /."""
  // How would this work for a cookie on /?  We will include it then.
  const std::string& url_path = url.path();

  size_t idx = url_path.find_last_of('/');

  // The cookie path was invalid or a single '/'.
  if (idx == 0 || idx == std::string::npos)
    return std::string("/");

  // Return up to the rightmost '/'.
  return url_path.substr(0, idx);
}

std::string CanonPath(const GURL& url,
                      const CookieMonster::ParsedCookie& pc) {
  std::string path_string;
  if (pc.HasPath())
    path_string = pc.Path();
  return CanonPathWithString(url, path_string);
}

Time CanonExpirationInternal(const CookieMonster::ParsedCookie& pc,
                             const Time& current) {
  // First, try the Max-Age attribute.
  uint64 max_age = 0;
  if (pc.HasMaxAge() &&
#ifdef COMPILER_MSVC
      sscanf_s(
#else
      sscanf(
#endif
             pc.MaxAge().c_str(), " %" PRIu64, &max_age) == 1) {
    return current + TimeDelta::FromSeconds(max_age);
  }

  // Try the Expires attribute.
  if (pc.HasExpires())
    return CookieMonster::ParseCookieTime(pc.Expires());

  // Invalid or no expiration, persistent cookie.
  return Time();
}

Time CanonExpiration(const CookieMonster::ParsedCookie& pc,
                     const Time& current,
                     const CookieOptions& options) {
  Time expiration_time = CanonExpirationInternal(pc, current);

  if (options.force_session()) {
    // Only override the expiry  adte if it's in the future. If the expiry date
    // is before the creation date, the cookie is supposed to be deleted.
    if (expiration_time.is_null() || expiration_time > current)
      return Time();
  }

  return expiration_time;
}

// Helper for GarbageCollection.  If |cookie_its->size() > num_max|, remove the
// |num_max - num_purge| most recently accessed cookies from cookie_its.
// (In other words, leave the entries that are candidates for
// eviction in cookie_its.)  The cookies returned will be in order sorted by
// access time, least recently accessed first.  The access time of the least
// recently accessed entry not returned will be placed in
// |*lra_removed| if that pointer is set.  FindLeastRecentlyAccessed
// returns false if no manipulation is done (because the list size is less
// than num_max), true otherwise.
bool FindLeastRecentlyAccessed(
    size_t num_max,
    size_t num_purge,
    Time* lra_removed,
    std::vector<CookieMonster::CookieMap::iterator>* cookie_its) {
  DCHECK_LE(num_purge, num_max);
  if (cookie_its->size() > num_max) {
    VLOG(kVlogGarbageCollection)
        << "FindLeastRecentlyAccessed() Deep Garbage Collect.";
    num_purge += cookie_its->size() - num_max;
    DCHECK_GT(cookie_its->size(), num_purge);

    // Add 1 so that we can get the last time left in the store.
    std::partial_sort(cookie_its->begin(), cookie_its->begin() + num_purge + 1,
                      cookie_its->end(), LRUCookieSorter);
    *lra_removed =
        (*(cookie_its->begin() + num_purge))->second->LastAccessDate();
    cookie_its->erase(cookie_its->begin() + num_purge, cookie_its->end());
    return true;
  }
  return false;
}

}  // namespace

// static
bool CookieMonster::enable_file_scheme_ = false;

CookieMonster::CookieMonster(PersistentCookieStore* store, Delegate* delegate)
    : initialized_(false),
      expiry_and_key_scheme_(expiry_and_key_default_),
      store_(store),
      last_access_threshold_(
          TimeDelta::FromSeconds(kDefaultAccessUpdateThresholdSeconds)),
      delegate_(delegate),
      last_statistic_record_time_(Time::Now()),
      keep_expired_cookies_(false),
      monster_alive_(kMonsterAlive) {
  InitializeHistograms();
  SetDefaultCookieableSchemes();
}

CookieMonster::CookieMonster(PersistentCookieStore* store,
                             Delegate* delegate,
                             int last_access_threshold_milliseconds)
    : initialized_(false),
      expiry_and_key_scheme_(expiry_and_key_default_),
      store_(store),
      last_access_threshold_(base::TimeDelta::FromMilliseconds(
          last_access_threshold_milliseconds)),
      delegate_(delegate),
      last_statistic_record_time_(base::Time::Now()),
      keep_expired_cookies_(false),
      monster_alive_(kMonsterAlive) {
  InitializeHistograms();
  SetDefaultCookieableSchemes();
}

// Parse a cookie expiration time.  We try to be lenient, but we need to
// assume some order to distinguish the fields.  The basic rules:
//  - The month name must be present and prefix the first 3 letters of the
//    full month name (jan for January, jun for June).
//  - If the year is <= 2 digits, it must occur after the day of month.
//  - The time must be of the format hh:mm:ss.
// An average cookie expiration will look something like this:
//   Sat, 15-Apr-17 21:01:22 GMT
Time CookieMonster::ParseCookieTime(const std::string& time_string) {
  static const char* kMonths[] = { "jan", "feb", "mar", "apr", "may", "jun",
                                   "jul", "aug", "sep", "oct", "nov", "dec" };
  static const int kMonthsLen = arraysize(kMonths);
  // We want to be pretty liberal, and support most non-ascii and non-digit
  // characters as a delimiter.  We can't treat : as a delimiter, because it
  // is the delimiter for hh:mm:ss, and we want to keep this field together.
  // We make sure to include - and +, since they could prefix numbers.
  // If the cookie attribute came in in quotes (ex expires="XXX"), the quotes
  // will be preserved, and we will get them here.  So we make sure to include
  // quote characters, and also \ for anything that was internally escaped.
  static const char* kDelimiters = "\t !\"#$%&'()*+,-./;<=>?@[\\]^_`{|}~";

  Time::Exploded exploded = {0};

  StringTokenizer tokenizer(time_string, kDelimiters);

  bool found_day_of_month = false;
  bool found_month = false;
  bool found_time = false;
  bool found_year = false;

  while (tokenizer.GetNext()) {
    const std::string token = tokenizer.token();
    DCHECK(!token.empty());
    bool numerical = IsAsciiDigit(token[0]);

    // String field
    if (!numerical) {
      if (!found_month) {
        for (int i = 0; i < kMonthsLen; ++i) {
          // Match prefix, so we could match January, etc
          if (base::strncasecmp(token.c_str(), kMonths[i], 3) == 0) {
            exploded.month = i + 1;
            found_month = true;
            break;
          }
        }
      } else {
        // If we've gotten here, it means we've already found and parsed our
        // month, and we have another string, which we would expect to be the
        // the time zone name.  According to the RFC and my experiments with
        // how sites format their expirations, we don't have much of a reason
        // to support timezones.  We don't want to ever barf on user input,
        // but this DCHECK should pass for well-formed data.
        // DCHECK(token == "GMT");
      }
    // Numeric field w/ a colon
    } else if (token.find(':') != std::string::npos) {
      if (!found_time &&
#ifdef COMPILER_MSVC
          sscanf_s(
#else
          sscanf(
#endif
                 token.c_str(), "%2u:%2u:%2u", &exploded.hour,
                 &exploded.minute, &exploded.second) == 3) {
        found_time = true;
      } else {
        // We should only ever encounter one time-like thing.  If we're here,
        // it means we've found a second, which shouldn't happen.  We keep
        // the first.  This check should be ok for well-formed input:
        // NOTREACHED();
      }
    // Numeric field
    } else {
      // Overflow with atoi() is unspecified, so we enforce a max length.
      if (!found_day_of_month && token.length() <= 2) {
        exploded.day_of_month = atoi(token.c_str());
        found_day_of_month = true;
      } else if (!found_year && token.length() <= 5) {
        exploded.year = atoi(token.c_str());
        found_year = true;
      } else {
        // If we're here, it means we've either found an extra numeric field,
        // or a numeric field which was too long.  For well-formed input, the
        // following check would be reasonable:
        // NOTREACHED();
      }
    }
  }

  if (!found_day_of_month || !found_month || !found_time || !found_year) {
    // We didn't find all of the fields we need.  For well-formed input, the
    // following check would be reasonable:
    // NOTREACHED() << "Cookie parse expiration failed: " << time_string;
    return Time();
  }

  // Normalize the year to expand abbreviated years to the full year.
  if (exploded.year >= 69 && exploded.year <= 99)
    exploded.year += 1900;
  if (exploded.year >= 0 && exploded.year <= 68)
    exploded.year += 2000;

  // If our values are within their correct ranges, we got our time.
  if (exploded.day_of_month >= 1 && exploded.day_of_month <= 31 &&
      exploded.month >= 1 && exploded.month <= 12 &&
      exploded.year >= 1601 && exploded.year <= 30827 &&
      exploded.hour <= 23 && exploded.minute <= 59 && exploded.second <= 59) {
    return Time::FromUTCExploded(exploded);
  }

  // One of our values was out of expected range.  For well-formed input,
  // the following check would be reasonable:
  // NOTREACHED() << "Cookie exploded expiration failed: " << time_string;

  return Time();
}

bool CookieMonster::DomainIsHostOnly(const std::string& domain_string) {
  return (domain_string.empty() || domain_string[0] != '.');
}

bool CookieMonster::SetCookieWithDetails(
    const GURL& url, const std::string& name, const std::string& value,
    const std::string& domain, const std::string& path,
    const base::Time& expiration_time, bool secure, bool http_only) {
  base::AutoLock autolock(lock_);
  ValidateMapWhileLockHeld(0);

  if (!HasCookieableScheme(url))
    return false;

  InitIfNecessary();

  Time creation_time = CurrentTime();
  last_time_seen_ = creation_time;

  scoped_ptr<CanonicalCookie> cc;
  cc.reset(CanonicalCookie::Create(
      url, name, value, domain, path,
      creation_time, expiration_time,
      secure, http_only));

  if (!cc.get())
    return false;

  CookieOptions options;
  options.set_include_httponly();
  return SetCanonicalCookie(&cc, creation_time, options);
}


CookieList CookieMonster::GetAllCookies() {
  base::AutoLock autolock(lock_);
  ValidateMapWhileLockHeld(0);
  InitIfNecessary();

  // This function is being called to scrape the cookie list for management UI
  // or similar.  We shouldn't show expired cookies in this list since it will
  // just be confusing to users, and this function is called rarely enough (and
  // is already slow enough) that it's OK to take the time to garbage collect
  // the expired cookies now.
  //
  // Note that this does not prune cookies to be below our limits (if we've
  // exceeded them) the way that calling GarbageCollect() would.
  GarbageCollectExpired(Time::Now(),
                        CookieMapItPair(cookies_.begin(), cookies_.end()),
                        NULL);

  // Copy the CanonicalCookie pointers from the map so that we can use the same
  // sorter as elsewhere, then copy the result out.
  std::vector<CanonicalCookie*> cookie_ptrs;
  cookie_ptrs.reserve(cookies_.size());
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end(); ++it)
    cookie_ptrs.push_back(it->second);
  std::sort(cookie_ptrs.begin(), cookie_ptrs.end(), CookieSorter);

  CookieList cookie_list;
  cookie_list.reserve(cookie_ptrs.size());
  for (std::vector<CanonicalCookie*>::const_iterator it = cookie_ptrs.begin();
       it != cookie_ptrs.end(); ++it)
    cookie_list.push_back(**it);

  ValidateMapWhileLockHeld(0);
  return cookie_list;
}

CookieList CookieMonster::GetAllCookiesForURLWithOptions(
    const GURL& url,
    const CookieOptions& options) {
  base::AutoLock autolock(lock_);
  ValidateMapWhileLockHeld(0);
  InitIfNecessary();

  std::vector<CanonicalCookie*> cookie_ptrs;
  FindCookiesForHostAndDomain(url, options, false, &cookie_ptrs);
  std::sort(cookie_ptrs.begin(), cookie_ptrs.end(), CookieSorter);

  CookieList cookies;
  for (std::vector<CanonicalCookie*>::const_iterator it = cookie_ptrs.begin();
       it != cookie_ptrs.end(); it++)
    cookies.push_back(**it);

  return cookies;
}

CookieList CookieMonster::GetAllCookiesForURL(const GURL& url) {
  CookieOptions options;
  options.set_include_httponly();

  return GetAllCookiesForURLWithOptions(url, options);
}

int CookieMonster::DeleteAll(bool sync_to_store) {
  base::AutoLock autolock(lock_);
  ValidateMapWhileLockHeld(0);
  if (sync_to_store)
    InitIfNecessary();

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    ++it;
    InternalDeleteCookie(curit, sync_to_store,
                         sync_to_store ? DELETE_COOKIE_EXPLICIT :
                             DELETE_COOKIE_DONT_RECORD /* Destruction. */);
    ++num_deleted;
  }

  return num_deleted;
}

int CookieMonster::DeleteAllCreatedBetween(const Time& delete_begin,
                                           const Time& delete_end,
                                           bool sync_to_store) {
  base::AutoLock autolock(lock_);
  ValidateMapWhileLockHeld(0);
  InitIfNecessary();

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    CanonicalCookie* cc = curit->second;
    ++it;

    if (cc->CreationDate() >= delete_begin &&
        (delete_end.is_null() || cc->CreationDate() < delete_end)) {
      InternalDeleteCookie(curit, sync_to_store, DELETE_COOKIE_EXPLICIT);
      ++num_deleted;
    }
  }

  return num_deleted;
}

int CookieMonster::DeleteAllCreatedAfter(const Time& delete_begin,
                                         bool sync_to_store) {
  return DeleteAllCreatedBetween(delete_begin, Time(), sync_to_store);
}

int CookieMonster::DeleteAllForHost(const GURL& url) {
  base::AutoLock autolock(lock_);
  ValidateMapWhileLockHeld(0);
  InitIfNecessary();

  if (!HasCookieableScheme(url))
    return 0;

  const std::string scheme(url.scheme());
  const std::string host(url.host());

  // We store host cookies in the store by their canonical host name;
  // domain cookies are stored with a leading ".".  So this is a pretty
  // simple lookup and per-cookie delete.
  int num_deleted = 0;
  for (CookieMapItPair its = cookies_.equal_range(GetKey(host));
       its.first != its.second;) {
    CookieMap::iterator curit = its.first;
    ++its.first;

    const CanonicalCookie* const cc = curit->second;

    // Delete only on a match as a host cookie.
    if (cc->IsHostCookie() && cc->IsDomainMatch(scheme, host)) {
      num_deleted++;

      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPLICIT);
    }
  }
  return num_deleted;
}

bool CookieMonster::DeleteCanonicalCookie(const CanonicalCookie& cookie) {
  base::AutoLock autolock(lock_);
  ValidateMapWhileLockHeld(0);
  InitIfNecessary();

  for (CookieMapItPair its = cookies_.equal_range(GetKey(cookie.Domain()));
       its.first != its.second; ++its.first) {
    // The creation date acts as our unique index...
    if (its.first->second->CreationDate() == cookie.CreationDate()) {
      InternalDeleteCookie(its.first, true, DELETE_COOKIE_EXPLICIT);
      return true;
    }
  }
  return false;
}

void CookieMonster::SetCookieableSchemes(
    const char* schemes[], size_t num_schemes) {
  base::AutoLock autolock(lock_);

  // Cookieable Schemes must be set before first use of function.
  DCHECK(!initialized_);

  cookieable_schemes_.clear();
  cookieable_schemes_.insert(cookieable_schemes_.end(),
                             schemes, schemes + num_schemes);
}

void CookieMonster::SetExpiryAndKeyScheme(ExpiryAndKeyScheme key_scheme) {
  DCHECK(!initialized_);
  expiry_and_key_scheme_ = key_scheme;
}

void CookieMonster::SetKeepExpiredCookies() {
  keep_expired_cookies_ = true;
}

void CookieMonster::SetClearPersistentStoreOnExit(bool clear_local_store) {
  if (store_)
    store_->SetClearLocalStateOnExit(clear_local_store);
}

// static
void CookieMonster::EnableFileScheme() {
  enable_file_scheme_ = true;
}

void CookieMonster::FlushStore(Task* completion_task) {
  base::AutoLock autolock(lock_);
  if (initialized_ && store_)
    store_->Flush(completion_task);
  else if (completion_task)
    MessageLoop::current()->PostTask(FROM_HERE, completion_task);
}

bool CookieMonster::SetCookieWithOptions(const GURL& url,
                                         const std::string& cookie_line,
                                         const CookieOptions& options) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url)) {
    return false;
  }

  InitIfNecessary();

  return SetCookieWithCreationTimeAndOptions(url, cookie_line, Time(), options);
}

std::string CookieMonster::GetCookiesWithOptions(const GURL& url,
                                                 const CookieOptions& options) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();

  if (!HasCookieableScheme(url)) {
    return std::string();
  }

  TimeTicks start_time(TimeTicks::Now());

  // Get the cookies for this host and its domain(s).
  std::vector<CanonicalCookie*> cookies;
  FindCookiesForHostAndDomain(url, options, true, &cookies);
  std::sort(cookies.begin(), cookies.end(), CookieSorter);

  std::string cookie_line;
  for (std::vector<CanonicalCookie*>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if (it != cookies.begin())
      cookie_line += "; ";
    // In Mozilla if you set a cookie like AAAA, it will have an empty token
    // and a value of AAAA.  When it sends the cookie back, it will send AAAA,
    // so we need to avoid sending =AAAA for a blank token value.
    if (!(*it)->Name().empty())
      cookie_line += (*it)->Name() + "=";
    cookie_line += (*it)->Value();
  }

  histogram_time_get_->AddTime(TimeTicks::Now() - start_time);

  VLOG(kVlogGetCookies) << "GetCookies() result: " << cookie_line;

  return cookie_line;
}

void CookieMonster::DeleteCookie(const GURL& url,
                                 const std::string& cookie_name) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();

  if (!HasCookieableScheme(url))
    return;

  CookieOptions options;
  options.set_include_httponly();
  // Get the cookies for this host and its domain(s).
  std::vector<CanonicalCookie*> cookies;
  FindCookiesForHostAndDomain(url, options, true, &cookies);
  std::set<CanonicalCookie*> matching_cookies;

  for (std::vector<CanonicalCookie*>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if ((*it)->Name() != cookie_name)
      continue;
    if (url.path().find((*it)->Path()))
      continue;
    matching_cookies.insert(*it);
  }

  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    ++it;
    if (matching_cookies.find(curit->second) != matching_cookies.end()) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPLICIT);
    }
  }
}

CookieMonster* CookieMonster::GetCookieMonster() {
  return this;
}

void CookieMonster::ValidateMap(int arg) {
  base::AutoLock autolock(lock_);
  ValidateMapWhileLockHeld(arg);
}

// TODO(eroman): Get rid of this once 74585 is fixed.
void AskUserToReportIssue() {
#if defined(OS_WIN)
  MessageBox(
      NULL,
      L"Chrome has crashed due to issue 74585.\n"
      L"Please help us fix the problem by adding a comment on:\n\n"
      L"    http://crbug.com/74585\n\n"
      L"(which explains how you triggered this.) Thanks!",
      L"OOPS!",
      MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
#endif  // defined(OS_WIN)
}


void CookieMonster::ValidateMapWhileLockHeld(int arg) {
  lock_.AssertAcquired();

  // Skipping InitIfNecessary() to allow validation before first use.
  CHECK_EQ(kMonsterAlive, monster_alive_);

  // Check if any of the pointers in the map are NULL (they shouldn't be).
  CookieMap::iterator it = cookies_.begin();
  int i = 0;

  for (; it != cookies_.end(); ++it, ++i) {
    if (!it->second) {
      // The map is invalid!
      AskUserToReportIssue();
      CrashBecauseCookieMapIsInvalid(cookies_, it, i);
    }
  }
}

// Disable optimizations so that our variables will be stack-resident in the
// minidumps.
#if defined(OS_WIN)
#pragma warning (disable: 4748)
#pragma optimize( "", off )
#endif

// TODO(eroman): Get rid of this once 74585 is fixed.
void CookieMonster::CrashBecauseCookieMapIsInvalid(
    const CookieMap& cookies,
    const CookieMap::iterator& null_value_it,
    int null_value_pos) {
  int num_null_values = 0;

  // Check how many other NULL values the map contains (in case there is more
  // than one).
  for (CookieMap::const_iterator it = cookies.begin(); it != cookies.end();
       ++it) {
    if (!it->second) {
      num_null_values += 1;
    }
  }

  // Keep track of how big the map was.
  int size = cookies.size();

  // Check that the non-NULL pointers are not garbage memory. If they are,
  // the following loop will likely crash when calling IsEquivalent()
  CanonicalCookie cc(
      GURL("http://bogus-cc/"), "bogus name", "bogus value", "bogus domain",
      "path", true, true, base::Time(), base::Time(), false, base::Time());
  for (CookieMap::const_iterator it = cookies.begin(); it != cookies.end();
       ++it) {
    if (it->second) {
      CHECK(!cc.IsEquivalent(*it->second));
    }
  }

  // Crash!
  CHECK(false);

  // The following shouldn't be necessary since we disabled optimizations, but
  // it can't hurt ;)
  LOG(INFO) << num_null_values << " " << size << " " << null_value_pos;
}

#if defined(OS_WIN)
#pragma optimize( "", on )
#pragma warning (default: 4748)
#endif

CookieMonster::~CookieMonster() {
  DeleteAll(false);
  monster_alive_ = kMonsterDead;
}

bool CookieMonster::SetCookieWithCreationTime(const GURL& url,
                                              const std::string& cookie_line,
                                              const base::Time& creation_time) {
  base::AutoLock autolock(lock_);
  CHECK_EQ(kMonsterAlive, monster_alive_);

  if (!HasCookieableScheme(url)) {
    return false;
  }

  InitIfNecessary();
  return SetCookieWithCreationTimeAndOptions(url, cookie_line, creation_time,
                                             CookieOptions());
}

void CookieMonster::InitStore() {
  DCHECK(store_) << "Store must exist to initialize";

  TimeTicks beginning_time(TimeTicks::Now());

  // Initialize the store and sync in any saved persistent cookies.  We don't
  // care if it's expired, insert it so it can be garbage collected, removed,
  // and sync'd.
  std::vector<CanonicalCookie*> cookies;
  // Reserve space for the maximum amount of cookies a database should have.
  // This prevents multiple vector growth / copies as we append cookies.
  cookies.reserve(kMaxCookies);
  store_->Load(&cookies);

  // Avoid ever letting cookies with duplicate creation times into the store;
  // that way we don't have to worry about what sections of code are safe
  // to call while it's in that state.
  std::set<int64> creation_times;

  // Presumably later than any access time in the store.
  Time earliest_access_time;

  for (std::vector<CanonicalCookie*>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    int64 cookie_creation_time = (*it)->CreationDate().ToInternalValue();

    if (creation_times.insert(cookie_creation_time).second) {
      InternalInsertCookie(GetKey((*it)->Domain()), *it, false);
      const Time cookie_access_time((*it)->LastAccessDate());
      if (earliest_access_time.is_null() ||
          cookie_access_time < earliest_access_time)
        earliest_access_time = cookie_access_time;
    } else {
      LOG(ERROR) << base::StringPrintf("Found cookies with duplicate creation "
                                       "times in backing store: "
                                       "{name='%s', domain='%s', path='%s'}",
                                       (*it)->Name().c_str(),
                                       (*it)->Domain().c_str(),
                                       (*it)->Path().c_str());
      // We've been given ownership of the cookie and are throwing it
      // away; reclaim the space.
      delete (*it);
    }
  }
  earliest_access_time_= earliest_access_time;

  // After importing cookies from the PersistentCookieStore, verify that
  // none of our other constraints are violated.
  //
  // In particular, the backing store might have given us duplicate cookies.
  EnsureCookiesMapIsValid();

  histogram_time_load_->AddTime(TimeTicks::Now() - beginning_time);
}

void CookieMonster::EnsureCookiesMapIsValid() {
  lock_.AssertAcquired();
  ValidateMapWhileLockHeld(0);

  int num_duplicates_trimmed = 0;

  // Iterate through all the of the cookies, grouped by host.
  CookieMap::iterator prev_range_end = cookies_.begin();
  while (prev_range_end != cookies_.end()) {
    CookieMap::iterator cur_range_begin = prev_range_end;
    const std::string key = cur_range_begin->first;  // Keep a copy.
    CookieMap::iterator cur_range_end = cookies_.upper_bound(key);
    prev_range_end = cur_range_end;

    // Ensure no equivalent cookies for this host.
    num_duplicates_trimmed +=
        TrimDuplicateCookiesForKey(key, cur_range_begin, cur_range_end);
  }

  // Record how many duplicates were found in the database.
  // See InitializeHistograms() for details.
  histogram_cookie_deletion_cause_->Add(num_duplicates_trimmed);
}

int CookieMonster::TrimDuplicateCookiesForKey(
    const std::string& key,
    CookieMap::iterator begin,
    CookieMap::iterator end) {
  lock_.AssertAcquired();
  ValidateMapWhileLockHeld(0);

  // Set of cookies ordered by creation time.
  typedef std::set<CookieMap::iterator, OrderByCreationTimeDesc> CookieSet;

  // Helper map we populate to find the duplicates.
  typedef std::map<CookieSignature, CookieSet> EquivalenceMap;
  EquivalenceMap equivalent_cookies;

  // The number of duplicate cookies that have been found.
  int num_duplicates = 0;

  // Iterate through all of the cookies in our range, and insert them into
  // the equivalence map.
  for (CookieMap::iterator it = begin; it != end; ++it) {
    DCHECK_EQ(key, it->first);
    CanonicalCookie* cookie = it->second;

    CookieSignature signature(cookie->Name(), cookie->Domain(),
                              cookie->Path());
    CookieSet& set = equivalent_cookies[signature];

    // We found a duplicate!
    if (!set.empty())
      num_duplicates++;

    // We save the iterator into |cookies_| rather than the actual cookie
    // pointer, since we may need to delete it later.
    bool insert_success = set.insert(it).second;
    DCHECK(insert_success) <<
        "Duplicate creation times found in duplicate cookie name scan.";
  }

  // If there were no duplicates, we are done!
  if (num_duplicates == 0)
    return 0;

  // Make sure we find everything below that we did above.
  int num_duplicates_found = 0;

  // Otherwise, delete all the duplicate cookies, both from our in-memory store
  // and from the backing store.
  for (EquivalenceMap::iterator it = equivalent_cookies.begin();
       it != equivalent_cookies.end();
       ++it) {
    const CookieSignature& signature = it->first;
    CookieSet& dupes = it->second;

    if (dupes.size() <= 1)
      continue;  // This cookiename/path has no duplicates.
    num_duplicates_found += dupes.size() - 1;

    // Since |dups| is sorted by creation time (descending), the first cookie
    // is the most recent one, so we will keep it. The rest are duplicates.
    dupes.erase(dupes.begin());

    LOG(ERROR) << base::StringPrintf(
        "Found %d duplicate cookies for host='%s', "
        "with {name='%s', domain='%s', path='%s'}",
        static_cast<int>(dupes.size()),
        key.c_str(),
        signature.name.c_str(),
        signature.domain.c_str(),
        signature.path.c_str());

    // Remove all the cookies identified by |dupes|. It is valid to delete our
    // list of iterators one at a time, since |cookies_| is a multimap (they
    // don't invalidate existing iterators following deletion).
    for (CookieSet::iterator dupes_it = dupes.begin();
         dupes_it != dupes.end();
         ++dupes_it) {
      InternalDeleteCookie(*dupes_it, true /*sync_to_store*/,
                           DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE);
    }
  }
  DCHECK_EQ(num_duplicates, num_duplicates_found);

  return num_duplicates;
}

// Note: file must be the last scheme.
const char* CookieMonster::kDefaultCookieableSchemes[] =
    { "http", "https", "file" };
const int CookieMonster::kDefaultCookieableSchemesCount =
    arraysize(CookieMonster::kDefaultCookieableSchemes);

void CookieMonster::SetDefaultCookieableSchemes() {
  int num_schemes = enable_file_scheme_ ?
      kDefaultCookieableSchemesCount : kDefaultCookieableSchemesCount - 1;
  SetCookieableSchemes(kDefaultCookieableSchemes, num_schemes);
}


void CookieMonster::FindCookiesForHostAndDomain(
    const GURL& url,
    const CookieOptions& options,
    bool update_access_time,
    std::vector<CanonicalCookie*>* cookies) {
  lock_.AssertAcquired();
  ValidateMapWhileLockHeld(0);

  const Time current_time(CurrentTime());

  // Probe to save statistics relatively frequently.  We do it here rather
  // than in the set path as many websites won't set cookies, and we
  // want to collect statistics whenever the browser's being used.
  RecordPeriodicStats(current_time);

  if (expiry_and_key_scheme_ == EKS_DISCARD_RECENT_AND_PURGE_DOMAIN) {
    // Can just dispatch to FindCookiesForKey
    const std::string key(GetKey(url.host()));
    FindCookiesForKey(key, url, options, current_time,
                      update_access_time, cookies);
  } else {
    // Need to probe for all domains that might have relevant
    // cookies for us.

    // Query for the full host, For example: 'a.c.blah.com'.
    std::string key(GetKey(url.host()));
    FindCookiesForKey(key, url, options, current_time, update_access_time,
                      cookies);

    // See if we can search for domain cookies, i.e. if the host has a TLD + 1.
    const std::string domain(GetEffectiveDomain(url.scheme(), key));
    if (domain.empty())
      return;
    DCHECK_LE(domain.length(), key.length());
    DCHECK_EQ(0, key.compare(key.length() - domain.length(), domain.length(),
                             domain));

    // Walk through the string and query at the dot points (GURL should have
    // canonicalized the dots, so this should be safe).  Stop once we reach the
    // domain + registry; we can't write cookies past this point, and with some
    // registrars other domains can, in which case we don't want to read their
    // cookies.
    for (key = "." + key; key.length() > domain.length(); ) {
      FindCookiesForKey(key, url, options, current_time, update_access_time,
                        cookies);
      const size_t next_dot = key.find('.', 1);  // Skip over leading dot.
      key.erase(0, next_dot);
    }
  }
}

void CookieMonster::FindCookiesForKey(
    const std::string& key,
    const GURL& url,
    const CookieOptions& options,
    const Time& current,
    bool update_access_time,
    std::vector<CanonicalCookie*>* cookies) {
  lock_.AssertAcquired();

  const std::string scheme(url.scheme());
  const std::string host(url.host());
  bool secure = url.SchemeIsSecure();

  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second; ) {
    CookieMap::iterator curit = its.first;
    CanonicalCookie* cc = curit->second;
    ++its.first;

    // If the cookie is expired, delete it.
    if (cc->IsExpired(current) && !keep_expired_cookies_) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPIRED);
      continue;
    }

    // Filter out HttpOnly cookies, per options.
    if (options.exclude_httponly() && cc->IsHttpOnly())
      continue;

    // Filter out secure cookies unless we're https.
    if (!secure && cc->IsSecure())
      continue;

    // Filter out cookies that don't apply to this domain.
    if (expiry_and_key_scheme_ == EKS_KEEP_RECENT_AND_PURGE_ETLDP1
        && !cc->IsDomainMatch(scheme, host))
      continue;

    if (!cc->IsOnPath(url.path()))
      continue;

    // Add this cookie to the set of matching cookies.  Update the access
    // time if we've been requested to do so.
    if (update_access_time) {
      InternalUpdateCookieAccessTime(cc, current);
    }
    cookies->push_back(cc);
  }
}

bool CookieMonster::DeleteAnyEquivalentCookie(const std::string& key,
                                              const CanonicalCookie& ecc,
                                              bool skip_httponly) {
  lock_.AssertAcquired();
  ValidateMapWhileLockHeld(0);

  bool found_equivalent_cookie = false;
  bool skipped_httponly = false;
  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second; ) {
    CookieMap::iterator curit = its.first;
    CanonicalCookie* cc = curit->second;
    ++its.first;

    if (ecc.IsEquivalent(*cc)) {
      // We should never have more than one equivalent cookie, since they should
      // overwrite each other.
      CHECK(!found_equivalent_cookie) <<
          "Duplicate equivalent cookies found, cookie store is corrupted.";
      if (skip_httponly && cc->IsHttpOnly()) {
        skipped_httponly = true;
      } else {
        InternalDeleteCookie(curit, true, DELETE_COOKIE_OVERWRITE);
      }
      found_equivalent_cookie = true;
    }
  }
  return skipped_httponly;
}

void CookieMonster::InternalInsertCookie(const std::string& key,
                                         CanonicalCookie* cc,
                                         bool sync_to_store) {
  lock_.AssertAcquired();
  ValidateMapWhileLockHeld(0);

  if (cc->IsPersistent() && store_ && sync_to_store)
    store_->AddCookie(*cc);
  cookies_.insert(CookieMap::value_type(key, cc));
  if (delegate_.get())
    delegate_->OnCookieChanged(*cc, false);
}

bool CookieMonster::SetCookieWithCreationTimeAndOptions(
    const GURL& url,
    const std::string& cookie_line,
    const Time& creation_time_or_null,
    const CookieOptions& options) {
  lock_.AssertAcquired();
  ValidateMapWhileLockHeld(0);

  VLOG(kVlogSetCookies) << "SetCookie() line: " << cookie_line;

  Time creation_time = creation_time_or_null;
  if (creation_time.is_null()) {
    creation_time = CurrentTime();
    last_time_seen_ = creation_time;
  }

  // Parse the cookie.
  ParsedCookie pc(cookie_line);

  if (!pc.IsValid()) {
    VLOG(kVlogSetCookies) << "WARNING: Couldn't parse cookie";
    return false;
  }

  if (options.exclude_httponly() && pc.IsHttpOnly()) {
    VLOG(kVlogSetCookies) << "SetCookie() not setting httponly cookie";
    return false;
  }

  std::string cookie_domain;
  if (!GetCookieDomain(url, pc, &cookie_domain)) {
    return false;
  }

  std::string cookie_path = CanonPath(url, pc);

  scoped_ptr<CanonicalCookie> cc;
  Time cookie_expires = CanonExpiration(pc, creation_time, options);

  cc.reset(new CanonicalCookie(url, pc.Name(),
                               pc.Value(), cookie_domain, cookie_path,
                               pc.IsSecure(), pc.IsHttpOnly(), creation_time,
                               creation_time, !cookie_expires.is_null(),
                               cookie_expires));

  if (!cc.get()) {
    VLOG(kVlogSetCookies) << "WARNING: Failed to allocate CanonicalCookie";
    return false;
  }
  return SetCanonicalCookie(&cc, creation_time, options);
}

bool CookieMonster::SetCanonicalCookie(scoped_ptr<CanonicalCookie>* cc,
                                       const Time& creation_time,
                                       const CookieOptions& options) {
  const std::string key(GetKey((*cc)->Domain()));
  if (DeleteAnyEquivalentCookie(key, **cc, options.exclude_httponly())) {
    VLOG(kVlogSetCookies) << "SetCookie() not clobbering httponly cookie";
    return false;
  }

  VLOG(kVlogSetCookies) << "SetCookie() key: " << key << " cc: "
                        << (*cc)->DebugString();

  // Realize that we might be setting an expired cookie, and the only point
  // was to delete the cookie which we've already done.
  if (!(*cc)->IsExpired(creation_time) || keep_expired_cookies_) {
    // See InitializeHistograms() for details.
    histogram_expiration_duration_minutes_->Add(
        ((*cc)->ExpiryDate() - creation_time).InMinutes());
    InternalInsertCookie(key, cc->release(), true);
  }

  // We assume that hopefully setting a cookie will be less common than
  // querying a cookie.  Since setting a cookie can put us over our limits,
  // make sure that we garbage collect...  We can also make the assumption that
  // if a cookie was set, in the common case it will be used soon after,
  // and we will purge the expired cookies in GetCookies().
  GarbageCollect(creation_time, key);

  return true;
}

void CookieMonster::InternalUpdateCookieAccessTime(CanonicalCookie* cc,
                                                   const Time& current) {
  lock_.AssertAcquired();
  ValidateMapWhileLockHeld(0);

  // Based off the Mozilla code.  When a cookie has been accessed recently,
  // don't bother updating its access time again.  This reduces the number of
  // updates we do during pageload, which in turn reduces the chance our storage
  // backend will hit its batch thresholds and be forced to update.
  if ((current - cc->LastAccessDate()) < last_access_threshold_)
    return;

  // See InitializeHistograms() for details.
  histogram_between_access_interval_minutes_->Add(
      (current - cc->LastAccessDate()).InMinutes());

  cc->SetLastAccessDate(current);
  if (cc->IsPersistent() && store_)
    store_->UpdateCookieAccessTime(*cc);
}

void CookieMonster::InternalDeleteCookie(CookieMap::iterator it,
                                         bool sync_to_store,
                                         DeletionCause deletion_cause) {
  lock_.AssertAcquired();

  // See InitializeHistograms() for details.
  if (deletion_cause != DELETE_COOKIE_DONT_RECORD)
    histogram_cookie_deletion_cause_->Add(deletion_cause);

  CanonicalCookie* cc = it->second;
  VLOG(kVlogSetCookies) << "InternalDeleteCookie() cc: " << cc->DebugString();

  if (cc->IsPersistent() && store_ && sync_to_store)
    store_->DeleteCookie(*cc);
  if (delegate_.get())
    delegate_->OnCookieChanged(*cc, true);
  cookies_.erase(it);
  delete cc;

  ValidateMapWhileLockHeld(0);
}

// Domain expiry behavior is unchanged by key/expiry scheme (the
// meaning of the key is different, but that's not visible to this
// routine).  Global garbage collection is dependent on key/expiry
// scheme in that recently touched cookies are not saved if
// expiry_and_key_scheme_ == EKS_DISCARD_RECENT_AND_PURGE_DOMAIN.
int CookieMonster::GarbageCollect(const Time& current,
                                  const std::string& key) {
  lock_.AssertAcquired();
  ValidateMapWhileLockHeld(0);

  int num_deleted = 0;

  // Collect garbage for this key.
  if (cookies_.count(key) > kDomainMaxCookies) {
    VLOG(kVlogGarbageCollection) << "GarbageCollect() key: " << key;

    std::vector<CookieMap::iterator> cookie_its;
    num_deleted += GarbageCollectExpired(
        current, cookies_.equal_range(key), &cookie_its);
    base::Time oldest_removed;
    if (FindLeastRecentlyAccessed(kDomainMaxCookies, kDomainPurgeCookies,
                                  &oldest_removed, &cookie_its)) {
      // Delete in two passes so we can figure out what we're nuking
      // that would be kept at the global level.
      int num_subject_to_global_purge =
          GarbageCollectDeleteList(
              current,
              Time::Now() - TimeDelta::FromDays(kSafeFromGlobalPurgeDays),
              DELETE_COOKIE_EVICTED_DOMAIN_PRE_SAFE,
              cookie_its);
      num_deleted += num_subject_to_global_purge;
      // Correct because FindLeastRecentlyAccessed returns a sorted list.
      cookie_its.erase(cookie_its.begin(),
                       cookie_its.begin() + num_subject_to_global_purge);
      num_deleted +=
          GarbageCollectDeleteList(
              current,
              Time(),
              DELETE_COOKIE_EVICTED_DOMAIN_POST_SAFE,
              cookie_its);
    }
  }

  // Collect garbage for everything.  With firefox style we want to
  // preserve cookies touched in kSafeFromGlobalPurgeDays, otherwise
  // not.
  if (cookies_.size() > kMaxCookies &&
      (expiry_and_key_scheme_ == EKS_DISCARD_RECENT_AND_PURGE_DOMAIN ||
       earliest_access_time_ <
       Time::Now() - TimeDelta::FromDays(kSafeFromGlobalPurgeDays))) {
    VLOG(kVlogGarbageCollection) << "GarbageCollect() everything";
    std::vector<CookieMap::iterator> cookie_its;
    base::Time oldest_left;
    num_deleted += GarbageCollectExpired(
        current, CookieMapItPair(cookies_.begin(), cookies_.end()),
        &cookie_its);
    if (FindLeastRecentlyAccessed(kMaxCookies, kPurgeCookies,
                                  &oldest_left, &cookie_its)) {
      Time oldest_safe_cookie(
          expiry_and_key_scheme_ == EKS_KEEP_RECENT_AND_PURGE_ETLDP1 ?
              (Time::Now() - TimeDelta::FromDays(kSafeFromGlobalPurgeDays)) :
              Time());                  // Null time == ignore access time.
      int num_evicted = GarbageCollectDeleteList(
          current,
          oldest_safe_cookie,
          DELETE_COOKIE_EVICTED_GLOBAL,
          cookie_its);

      // If no cookies were preserved by the time limit, the global last
      // access is set to the value returned from FindLeastRecentlyAccessed.
      // If the time limit preserved some cookies, we use the last access of
      // the oldest preserved cookie.
      if (num_evicted == static_cast<int>(cookie_its.size())) {
        earliest_access_time_ = oldest_left;
      } else {
        earliest_access_time_ =
            (*(cookie_its.begin() + num_evicted))->second->LastAccessDate();
      }
      num_deleted += num_evicted;
    }
  }

  ValidateMapWhileLockHeld(0);

  return num_deleted;
}

int CookieMonster::GarbageCollectExpired(
    const Time& current,
    const CookieMapItPair& itpair,
    std::vector<CookieMap::iterator>* cookie_its) {
  if (keep_expired_cookies_)
    return 0;

  lock_.AssertAcquired();

  int num_deleted = 0;
  for (CookieMap::iterator it = itpair.first, end = itpair.second; it != end;) {
    CookieMap::iterator curit = it;
    ++it;

    if (curit->second->IsExpired(current)) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPIRED);
      ++num_deleted;
    } else if (cookie_its) {
      cookie_its->push_back(curit);
    }
  }

  return num_deleted;
}

int CookieMonster::GarbageCollectDeleteList(
    const Time& current,
    const Time& keep_accessed_after,
    DeletionCause cause,
    std::vector<CookieMap::iterator>& cookie_its) {
  int num_deleted = 0;
  for (std::vector<CookieMap::iterator>::iterator it = cookie_its.begin();
       it != cookie_its.end(); it++) {
    if (keep_accessed_after.is_null() ||
        (*it)->second->LastAccessDate() < keep_accessed_after) {
      histogram_evicted_last_access_minutes_->Add(
          (current - (*it)->second->LastAccessDate()).InMinutes());
      InternalDeleteCookie((*it), true, cause);
      num_deleted++;
    }
  }
  return num_deleted;
}

// A wrapper around RegistryControlledDomainService::GetDomainAndRegistry
// to make clear we're creating a key for our local map.  Here and
// in FindCookiesForHostAndDomain() are the only two places where
// we need to conditionalize based on key type.
//
// Note that this key algorithm explicitly ignores the scheme.  This is
// because when we're entering cookies into the map from the backing store,
// we in general won't have the scheme at that point.
// In practical terms, this means that file cookies will be stored
// in the map either by an empty string or by UNC name (and will be
// limited by kMaxCookiesPerHost), and extension cookies will be stored
// based on the single extension id, as the extension id won't have the
// form of a DNS host and hence GetKey() will return it unchanged.
//
// Arguably the right thing to do here is to make the key
// algorithm dependent on the scheme, and make sure that the scheme is
// available everywhere the key must be obtained (specfically at backing
// store load time).  This would require either changing the backing store
// database schema to include the scheme (far more trouble than it's worth), or
// separating out file cookies into their own CookieMonster instance and
// thus restricting each scheme to a single cookie monster (which might
// be worth it, but is still too much trouble to solve what is currently a
// non-problem).
std::string CookieMonster::GetKey(const std::string& domain) const {
  if (expiry_and_key_scheme_ == EKS_DISCARD_RECENT_AND_PURGE_DOMAIN)
    return domain;

  std::string effective_domain(
      RegistryControlledDomainService::GetDomainAndRegistry(domain));
  if (effective_domain.empty())
    effective_domain = domain;

  if (!effective_domain.empty() && effective_domain[0] == '.')
    return effective_domain.substr(1);
  return effective_domain;
}

bool CookieMonster::HasCookieableScheme(const GURL& url) {
  lock_.AssertAcquired();

  // Make sure the request is on a cookie-able url scheme.
  for (size_t i = 0; i < cookieable_schemes_.size(); ++i) {
    // We matched a scheme.
    if (url.SchemeIs(cookieable_schemes_[i].c_str())) {
      // We've matched a supported scheme.
      return true;
    }
  }

  // The scheme didn't match any in our whitelist.
  VLOG(kVlogPerCookieMonster) << "WARNING: Unsupported cookie scheme: "
                              << url.scheme();
  return false;
}

// Test to see if stats should be recorded, and record them if so.
// The goal here is to get sampling for the average browser-hour of
// activity.  We won't take samples when the web isn't being surfed,
// and when the web is being surfed, we'll take samples about every
// kRecordStatisticsIntervalSeconds.
// last_statistic_record_time_ is initialized to Now() rather than null
// in the constructor so that we won't take statistics right after
// startup, to avoid bias from browsers that are started but not used.
void CookieMonster::RecordPeriodicStats(const base::Time& current_time) {
  const base::TimeDelta kRecordStatisticsIntervalTime(
      base::TimeDelta::FromSeconds(kRecordStatisticsIntervalSeconds));

  // If we've taken statistics recently, return.
  if (current_time - last_statistic_record_time_ <=
      kRecordStatisticsIntervalTime) {
    return;
  }

  // See InitializeHistograms() for details.
  histogram_count_->Add(cookies_.size());

  // More detailed statistics on cookie counts at different granularities.
  TimeTicks beginning_of_time(TimeTicks::Now());

  for (CookieMap::const_iterator it_key = cookies_.begin();
       it_key != cookies_.end(); ) {
    const std::string& key(it_key->first);

    int key_count = 0;
    typedef std::map<std::string, unsigned int> DomainMap;
    DomainMap domain_map;
    CookieMapItPair its_cookies = cookies_.equal_range(key);
    while (its_cookies.first != its_cookies.second) {
      key_count++;
      const std::string& cookie_domain(its_cookies.first->second->Domain());
      domain_map[cookie_domain]++;

      its_cookies.first++;
    }
    histogram_etldp1_count_->Add(key_count);
    histogram_domain_per_etldp1_count_->Add(domain_map.size());
    for (DomainMap::const_iterator domain_map_it = domain_map.begin();
         domain_map_it != domain_map.end(); domain_map_it++)
      histogram_domain_count_->Add(domain_map_it->second);

    it_key = its_cookies.second;
  }

  VLOG(kVlogPeriodic)
      << "Time for recording cookie stats (us): "
      << (TimeTicks::Now() - beginning_of_time).InMicroseconds();

  last_statistic_record_time_ = current_time;
}

// Initialize all histogram counter variables used in this class.
//
// Normal histogram usage involves using the macros defined in
// histogram.h, which automatically takes care of declaring these
// variables (as statics), initializing them, and accumulating into
// them, all from a single entry point.  Unfortunately, that solution
// doesn't work for the CookieMonster, as it's vulnerable to races between
// separate threads executing the same functions and hence initializing the
// same static variables.  There isn't a race danger in the histogram
// accumulation calls; they are written to be resilient to simultaneous
// calls from multiple threads.
//
// The solution taken here is to have per-CookieMonster instance
// variables that are constructed during CookieMonster construction.
// Note that these variables refer to the same underlying histogram,
// so we still race (but safely) with other CookieMonster instances
// for accumulation.
//
// To do this we've expanded out the individual histogram macros calls,
// with declarations of the variables in the class decl, initialization here
// (done from the class constructor) and direct calls to the accumulation
// methods where needed.  The specific histogram macro calls on which the
// initialization is based are included in comments below.
void CookieMonster::InitializeHistograms() {
  // From UMA_HISTOGRAM_CUSTOM_COUNTS
  histogram_expiration_duration_minutes_ = base::Histogram::FactoryGet(
      "Cookie.ExpirationDurationMinutes",
      1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_between_access_interval_minutes_ = base::Histogram::FactoryGet(
      "Cookie.BetweenAccessIntervalMinutes",
      1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_evicted_last_access_minutes_ = base::Histogram::FactoryGet(
      "Cookie.EvictedLastAccessMinutes",
      1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_count_ = base::Histogram::FactoryGet(
      "Cookie.Count", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_domain_count_ = base::Histogram::FactoryGet(
      "Cookie.DomainCount", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_etldp1_count_ = base::Histogram::FactoryGet(
      "Cookie.Etldp1Count", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_domain_per_etldp1_count_ = base::Histogram::FactoryGet(
      "Cookie.DomainPerEtldp1Count", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_COUNTS_10000 & UMA_HISTOGRAM_CUSTOM_COUNTS
  histogram_number_duplicate_db_cookies_ = base::Histogram::FactoryGet(
      "Net.NumDuplicateCookiesInDb", 1, 10000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_ENUMERATION
  histogram_cookie_deletion_cause_ = base::LinearHistogram::FactoryGet(
      "Cookie.DeletionCause", 1,
      DELETE_COOKIE_LAST_ENTRY - 1, DELETE_COOKIE_LAST_ENTRY,
      base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_{CUSTOM_,}TIMES
  histogram_time_get_ = base::Histogram::FactoryTimeGet("Cookie.TimeGet",
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(1),
      50, base::Histogram::kUmaTargetedHistogramFlag);
  histogram_time_load_ = base::Histogram::FactoryTimeGet("Cookie.TimeLoad",
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(1),
      50, base::Histogram::kUmaTargetedHistogramFlag);
}


// The system resolution is not high enough, so we can have multiple
// set cookies that result in the same system time.  When this happens, we
// increment by one Time unit.  Let's hope computers don't get too fast.
Time CookieMonster::CurrentTime() {
  return std::max(Time::Now(),
      Time::FromInternalValue(last_time_seen_.ToInternalValue() + 1));
}

CookieMonster::ParsedCookie::ParsedCookie(const std::string& cookie_line)
    : is_valid_(false),
      path_index_(0),
      domain_index_(0),
      expires_index_(0),
      maxage_index_(0),
      secure_index_(0),
      httponly_index_(0) {

  if (cookie_line.size() > kMaxCookieSize) {
    VLOG(1) << "Not parsing cookie, too large: " << cookie_line.size();
    return;
  }

  ParseTokenValuePairs(cookie_line);
  if (!pairs_.empty()) {
    is_valid_ = true;
    SetupAttributes();
  }
}

CookieMonster::ParsedCookie::~ParsedCookie() {
}

// Returns true if |c| occurs in |chars|
// TODO maybe make this take an iterator, could check for end also?
static inline bool CharIsA(const char c, const char* chars) {
  return strchr(chars, c) != NULL;
}
// Seek the iterator to the first occurrence of a character in |chars|.
// Returns true if it hit the end, false otherwise.
static inline bool SeekTo(std::string::const_iterator* it,
                          const std::string::const_iterator& end,
                          const char* chars) {
  for (; *it != end && !CharIsA(**it, chars); ++(*it)) {}
  return *it == end;
}
// Seek the iterator to the first occurrence of a character not in |chars|.
// Returns true if it hit the end, false otherwise.
static inline bool SeekPast(std::string::const_iterator* it,
                            const std::string::const_iterator& end,
                            const char* chars) {
  for (; *it != end && CharIsA(**it, chars); ++(*it)) {}
  return *it == end;
}
static inline bool SeekBackPast(std::string::const_iterator* it,
                                const std::string::const_iterator& end,
                                const char* chars) {
  for (; *it != end && CharIsA(**it, chars); --(*it)) {}
  return *it == end;
}

const char CookieMonster::ParsedCookie::kTerminator[] = "\n\r\0";
const int CookieMonster::ParsedCookie::kTerminatorLen =
    sizeof(kTerminator) - 1;
const char CookieMonster::ParsedCookie::kWhitespace[] = " \t";
const char CookieMonster::ParsedCookie::kValueSeparator[] = ";";
const char CookieMonster::ParsedCookie::kTokenSeparator[] = ";=";

// Create a cookie-line for the cookie.  For debugging only!
// If we want to use this for something more than debugging, we
// should rewrite it better...
std::string CookieMonster::ParsedCookie::DebugString() const {
  std::string out;
  for (PairList::const_iterator it = pairs_.begin();
       it != pairs_.end(); ++it) {
    out.append(it->first);
    out.append("=");
    out.append(it->second);
    out.append("; ");
  }
  return out;
}

std::string::const_iterator CookieMonster::ParsedCookie::FindFirstTerminator(
    const std::string& s) {
  std::string::const_iterator end = s.end();
  size_t term_pos =
      s.find_first_of(std::string(kTerminator, kTerminatorLen));
  if (term_pos != std::string::npos) {
    // We found a character we should treat as an end of string.
    end = s.begin() + term_pos;
  }
  return end;
}

bool CookieMonster::ParsedCookie::ParseToken(
    std::string::const_iterator* it,
    const std::string::const_iterator& end,
    std::string::const_iterator* token_start,
    std::string::const_iterator* token_end) {
  DCHECK(it && token_start && token_end);
  std::string::const_iterator token_real_end;

  // Seek past any whitespace before the "token" (the name).
  // token_start should point at the first character in the token
  if (SeekPast(it, end, kWhitespace))
    return false;  // No token, whitespace or empty.
  *token_start = *it;

  // Seek over the token, to the token separator.
  // token_real_end should point at the token separator, i.e. '='.
  // If it == end after the seek, we probably have a token-value.
  SeekTo(it, end, kTokenSeparator);
  token_real_end = *it;

  // Ignore any whitespace between the token and the token separator.
  // token_end should point after the last interesting token character,
  // pointing at either whitespace, or at '=' (and equal to token_real_end).
  if (*it != *token_start) {  // We could have an empty token name.
    --(*it);  // Go back before the token separator.
    // Skip over any whitespace to the first non-whitespace character.
    SeekBackPast(it, *token_start, kWhitespace);
    // Point after it.
    ++(*it);
  }
  *token_end = *it;

  // Seek us back to the end of the token.
  *it = token_real_end;
  return true;
}

void CookieMonster::ParsedCookie::ParseValue(
    std::string::const_iterator* it,
    const std::string::const_iterator& end,
    std::string::const_iterator* value_start,
    std::string::const_iterator* value_end) {
  DCHECK(it && value_start && value_end);

  // Seek past any whitespace that might in-between the token and value.
  SeekPast(it, end, kWhitespace);
  // value_start should point at the first character of the value.
  *value_start = *it;

  // It is unclear exactly how quoted string values should be handled.
  // Major browsers do different things, for example, Firefox supports
  // semicolons embedded in a quoted value, while IE does not.  Looking at
  // the specs, RFC 2109 and 2965 allow for a quoted-string as the value.
  // However, these specs were apparently written after browsers had
  // implemented cookies, and they seem very distant from the reality of
  // what is actually implemented and used on the web.  The original spec
  // from Netscape is possibly what is closest to the cookies used today.
  // This spec didn't have explicit support for double quoted strings, and
  // states that ; is not allowed as part of a value.  We had originally
  // implement the Firefox behavior (A="B;C"; -> A="B;C";).  However, since
  // there is no standard that makes sense, we decided to follow the behavior
  // of IE and Safari, which is closer to the original Netscape proposal.
  // This means that A="B;C" -> A="B;.  This also makes the code much simpler
  // and reduces the possibility for invalid cookies, where other browsers
  // like Opera currently reject those invalid cookies (ex A="B" "C";).

  // Just look for ';' to terminate ('=' allowed).
  // We can hit the end, maybe they didn't terminate.
  SeekTo(it, end, kValueSeparator);

  // Will be pointed at the ; seperator or the end.
  *value_end = *it;

  // Ignore any unwanted whitespace after the value.
  if (*value_end != *value_start) {  // Could have an empty value
    --(*value_end);
    SeekBackPast(value_end, *value_start, kWhitespace);
    ++(*value_end);
  }
}

std::string CookieMonster::ParsedCookie::ParseTokenString(
    const std::string& token) {
  std::string::const_iterator it = token.begin();
  std::string::const_iterator end = FindFirstTerminator(token);

  std::string::const_iterator token_start, token_end;
  if (ParseToken(&it, end, &token_start, &token_end))
    return std::string(token_start, token_end);
  return std::string();
}

std::string CookieMonster::ParsedCookie::ParseValueString(
    const std::string& value) {
  std::string::const_iterator it = value.begin();
  std::string::const_iterator end = FindFirstTerminator(value);

  std::string::const_iterator value_start, value_end;
  ParseValue(&it, end, &value_start, &value_end);
  return std::string(value_start, value_end);
}

// Parse all token/value pairs and populate pairs_.
void CookieMonster::ParsedCookie::ParseTokenValuePairs(
    const std::string& cookie_line) {
  pairs_.clear();

  // Ok, here we go.  We should be expecting to be starting somewhere
  // before the cookie line, not including any header name...
  std::string::const_iterator start = cookie_line.begin();
  std::string::const_iterator it = start;

  // TODO Make sure we're stripping \r\n in the network code.  Then we
  // can log any unexpected terminators.
  std::string::const_iterator end = FindFirstTerminator(cookie_line);

  for (int pair_num = 0; pair_num < kMaxPairs && it != end; ++pair_num) {
    TokenValuePair pair;

    std::string::const_iterator token_start, token_end;
    if (!ParseToken(&it, end, &token_start, &token_end))
      break;

    if (it == end || *it != '=') {
      // We have a token-value, we didn't have any token name.
      if (pair_num == 0) {
        // For the first time around, we want to treat single values
        // as a value with an empty name. (Mozilla bug 169091).
        // IE seems to also have this behavior, ex "AAA", and "AAA=10" will
        // set 2 different cookies, and setting "BBB" will then replace "AAA".
        pair.first = "";
        // Rewind to the beginning of what we thought was the token name,
        // and let it get parsed as a value.
        it = token_start;
      } else {
        // Any not-first attribute we want to treat a value as a
        // name with an empty value...  This is so something like
        // "secure;" will get parsed as a Token name, and not a value.
        pair.first = std::string(token_start, token_end);
      }
    } else {
      // We have a TOKEN=VALUE.
      pair.first = std::string(token_start, token_end);
      ++it;  // Skip past the '='.
    }

    // OK, now try to parse a value.
    std::string::const_iterator value_start, value_end;
    ParseValue(&it, end, &value_start, &value_end);
    // OK, we're finished with a Token/Value.
    pair.second = std::string(value_start, value_end);

    // From RFC2109: "Attributes (names) (attr) are case-insensitive."
    if (pair_num != 0)
      StringToLowerASCII(&pair.first);
    pairs_.push_back(pair);

    // We've processed a token/value pair, we're either at the end of
    // the string or a ValueSeparator like ';', which we want to skip.
    if (it != end)
      ++it;
  }
}

void CookieMonster::ParsedCookie::SetupAttributes() {
  static const char kPathTokenName[]      = "path";
  static const char kDomainTokenName[]    = "domain";
  static const char kExpiresTokenName[]   = "expires";
  static const char kMaxAgeTokenName[]    = "max-age";
  static const char kSecureTokenName[]    = "secure";
  static const char kHttpOnlyTokenName[]  = "httponly";

  // We skip over the first token/value, the user supplied one.
  for (size_t i = 1; i < pairs_.size(); ++i) {
    if (pairs_[i].first == kPathTokenName) {
      path_index_ = i;
    } else if (pairs_[i].first == kDomainTokenName) {
      domain_index_ = i;
    } else if (pairs_[i].first == kExpiresTokenName) {
      expires_index_ = i;
    } else if (pairs_[i].first == kMaxAgeTokenName) {
      maxage_index_ = i;
    } else if (pairs_[i].first == kSecureTokenName) {
      secure_index_ = i;
    } else if (pairs_[i].first == kHttpOnlyTokenName) {
      httponly_index_ = i;
    } else {
      /* some attribute we don't know or don't care about. */
    }
  }
}

CookieMonster::CanonicalCookie::CanonicalCookie()
    : has_expires_(false),
      secure_(false),
      httponly_(false) {
}

CookieMonster::CanonicalCookie::CanonicalCookie(const GURL& url,
                                                const std::string& name,
                                                const std::string& value,
                                                const std::string& domain,
                                                const std::string& path,
                                                bool secure,
                                                bool httponly,
                                                const base::Time& creation,
                                                const base::Time& last_access,
                                                bool has_expires,
                                                const base::Time& expires)
    : source_(GetCookieSourceFromURL(url)),
      name_(name),
      value_(value),
      domain_(domain),
      path_(path),
      creation_date_(creation),
      last_access_date_(last_access),
      expiry_date_(expires),
      has_expires_(has_expires),
      secure_(secure),
      httponly_(httponly) {
}

CookieMonster::CanonicalCookie::CanonicalCookie(const GURL& url,
                                                const ParsedCookie& pc)
    : source_(GetCookieSourceFromURL(url)),
      name_(pc.Name()),
      value_(pc.Value()),
      path_(CanonPath(url, pc)),
      creation_date_(Time::Now()),
      last_access_date_(Time()),
      has_expires_(pc.HasExpires()),
      secure_(pc.IsSecure()),
      httponly_(pc.IsHttpOnly()) {
  if (has_expires_)
    expiry_date_ = CanonExpiration(pc, creation_date_, CookieOptions());

  // Do the best we can with the domain.
  std::string cookie_domain;
  std::string domain_string;
  if (pc.HasDomain()) {
    domain_string = pc.Domain();
  }
  bool result
      = GetCookieDomainWithString(url, domain_string,
                                  &cookie_domain);
  // Caller is responsible for passing in good arguments.
  DCHECK(result);
  domain_ = cookie_domain;
}

CookieMonster::CanonicalCookie::~CanonicalCookie() {
}

std::string CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
    const GURL& url) {
  if (url.SchemeIsFile())
    return url.spec();

  url_canon::Replacements<char> replacements;
  replacements.ClearPort();
  if (url.SchemeIsSecure())
    replacements.SetScheme("http", url_parse::Component(0, 4));

  return url.GetOrigin().ReplaceComponents(replacements).spec();
}

CookieMonster::CanonicalCookie* CookieMonster::CanonicalCookie::Create(
      const GURL& url, const std::string& name, const std::string& value,
      const std::string& domain, const std::string& path,
      const base::Time& creation_time, const base::Time& expiration_time,
      bool secure, bool http_only) {
  // Expect valid attribute tokens and values, as defined by the ParsedCookie
  // logic, otherwise don't create the cookie.
  std::string parsed_name = ParsedCookie::ParseTokenString(name);
  if (parsed_name != name)
    return NULL;
  std::string parsed_value = ParsedCookie::ParseValueString(value);
  if (parsed_value != value)
    return NULL;

  std::string parsed_domain = ParsedCookie::ParseValueString(domain);
  if (parsed_domain != domain)
    return NULL;
  std::string cookie_domain;
  if (!GetCookieDomainWithString(url, parsed_domain, &cookie_domain))
    return NULL;

  std::string parsed_path = ParsedCookie::ParseValueString(path);
  if (parsed_path != path)
    return NULL;

  std::string cookie_path = CanonPathWithString(url, parsed_path);
  // Expect that the path was either not specified (empty), or is valid.
  if (!parsed_path.empty() && cookie_path != parsed_path)
    return NULL;
  // Canonicalize path again to make sure it escapes characters as needed.
  url_parse::Component path_component(0, cookie_path.length());
  url_canon::RawCanonOutputT<char> canon_path;
  url_parse::Component canon_path_component;
  url_canon::CanonicalizePath(cookie_path.data(), path_component,
                              &canon_path, &canon_path_component);
  cookie_path = std::string(canon_path.data() + canon_path_component.begin,
                            canon_path_component.len);

  return new CanonicalCookie(url, parsed_name,
                             parsed_value, cookie_domain, cookie_path, secure,
                             http_only, creation_time, creation_time,
                             !expiration_time.is_null(), expiration_time);
}

bool CookieMonster::CanonicalCookie::IsOnPath(
    const std::string& url_path) const {

  // A zero length would be unsafe for our trailing '/' checks, and
  // would also make no sense for our prefix match.  The code that
  // creates a CanonicalCookie should make sure the path is never zero length,
  // but we double check anyway.
  if (path_.empty())
    return false;

  // The Mozilla code broke it into 3 cases, if it's strings lengths
  // are less than, equal, or greater.  I think this is simpler:

  // Make sure the cookie path is a prefix of the url path.  If the
  // url path is shorter than the cookie path, then the cookie path
  // can't be a prefix.
  if (url_path.find(path_) != 0)
    return false;

  // Now we know that url_path is >= cookie_path, and that cookie_path
  // is a prefix of url_path.  If they are the are the same length then
  // they are identical, otherwise we need an additional check:

  // In order to avoid in correctly matching a cookie path of /blah
  // with a request path of '/blahblah/', we need to make sure that either
  // the cookie path ends in a trailing '/', or that we prefix up to a '/'
  // in the url path.  Since we know that the url path length is greater
  // than the cookie path length, it's safe to index one byte past.
  if (path_.length() != url_path.length() &&
      path_[path_.length() - 1] != '/' &&
      url_path[path_.length()] != '/')
    return false;

  return true;
}

bool CookieMonster::CanonicalCookie::IsDomainMatch(
    const std::string& scheme,
    const std::string& host) const {
  // Can domain match in two ways; as a domain cookie (where the cookie
  // domain begins with ".") or as a host cookie (where it doesn't).

  // Some consumers of the CookieMonster expect to set cookies on
  // URLs like http://.strange.url.  To retrieve cookies in this instance,
  // we allow matching as a host cookie even when the domain_ starts with
  // a period.
  if (host == domain_)
    return true;

  // Domain cookie must have an initial ".".  To match, it must be
  // equal to url's host with initial period removed, or a suffix of
  // it.

  // Arguably this should only apply to "http" or "https" cookies, but
  // extension cookie tests currently use the funtionality, and if we
  // ever decide to implement that it should be done by preventing
  // such cookies from being set.
  if (domain_.empty() || domain_[0] != '.')
    return false;

  // The host with a "." prefixed.
  if (domain_.compare(1, std::string::npos, host) == 0)
    return true;

  // A pure suffix of the host (ok since we know the domain already
  // starts with a ".")
  return (host.length() > domain_.length() &&
          host.compare(host.length() - domain_.length(),
                       domain_.length(), domain_) == 0);
}

std::string CookieMonster::CanonicalCookie::DebugString() const {
  return base::StringPrintf(
      "name: %s value: %s domain: %s path: %s creation: %"
      PRId64,
      name_.c_str(), value_.c_str(),
      domain_.c_str(), path_.c_str(),
      static_cast<int64>(creation_date_.ToTimeT()));
}

}  // namespace
