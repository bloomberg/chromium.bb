// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

const char kGlobalCookieSetURL[] = "chrome://cookieset";

void OnFetchComplete(const BrowsingDataCookieHelper::FetchCallback& callback,
                     const net::CookieList& cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, cookies));
}

}  // namespace

BrowsingDataCookieHelper::BrowsingDataCookieHelper(
    net::URLRequestContextGetter* request_context_getter)
    : request_context_getter_(request_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BrowsingDataCookieHelper::~BrowsingDataCookieHelper() {
}

void BrowsingDataCookieHelper::StartFetching(const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BrowsingDataCookieHelper::FetchCookiesOnIOThread, this,
                 callback));
}

void BrowsingDataCookieHelper::DeleteCookie(
    const net::CanonicalCookie& cookie) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BrowsingDataCookieHelper::DeleteCookieOnIOThread,
                 this, cookie));
}

void BrowsingDataCookieHelper::FetchCookiesOnIOThread(
    const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());
  request_context_getter_->GetURLRequestContext()->cookie_store()->
      GetAllCookiesAsync(base::Bind(&OnFetchComplete, callback));
}

void BrowsingDataCookieHelper::DeleteCookieOnIOThread(
    const net::CanonicalCookie& cookie) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  request_context_getter_->GetURLRequestContext()->cookie_store()->
      DeleteCanonicalCookieAsync(
          cookie, net::CookieStore::DeleteCallback());
}

CannedBrowsingDataCookieHelper::CannedBrowsingDataCookieHelper(
    net::URLRequestContextGetter* request_context_getter)
    : BrowsingDataCookieHelper(request_context_getter) {
}

CannedBrowsingDataCookieHelper::~CannedBrowsingDataCookieHelper() {
  Reset();
}

void CannedBrowsingDataCookieHelper::AddReadCookies(
    const GURL& frame_url,
    const GURL& url,
    const net::CookieList& cookie_list) {
  for (const auto& add_cookie : cookie_list)
    AddCookie(frame_url, add_cookie);
}

void CannedBrowsingDataCookieHelper::AddChangedCookie(
    const GURL& frame_url,
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options) {
  scoped_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
      url, cookie_line, base::Time::Now(), options));
  if (cookie.get())
    AddCookie(frame_url, *cookie);
}

void CannedBrowsingDataCookieHelper::Reset() {
  STLDeleteContainerPairSecondPointers(origin_cookie_set_map_.begin(),
                                       origin_cookie_set_map_.end());
  origin_cookie_set_map_.clear();
}

bool CannedBrowsingDataCookieHelper::empty() const {
  for (const auto& pair : origin_cookie_set_map_) {
    if (!pair.second->empty())
      return false;
  }
  return true;
}


size_t CannedBrowsingDataCookieHelper::GetCookieCount() const {
  size_t count = 0;
  for (const auto& pair : origin_cookie_set_map_)
    count += pair.second->size();
  return count;
}


void CannedBrowsingDataCookieHelper::StartFetching(
    const net::CookieStore::GetCookieListCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  net::CookieList cookie_list;
  for (const auto& pair : origin_cookie_set_map_) {
    cookie_list.insert(cookie_list.begin(), pair.second->begin(),
                       pair.second->end());
  }
  callback.Run(cookie_list);
}

void CannedBrowsingDataCookieHelper::DeleteCookie(
    const net::CanonicalCookie& cookie) {
  for (const auto& pair : origin_cookie_set_map_)
    DeleteMatchingCookie(cookie, pair.second);
  BrowsingDataCookieHelper::DeleteCookie(cookie);
}

bool CannedBrowsingDataCookieHelper::DeleteMatchingCookie(
    const net::CanonicalCookie& add_cookie,
    canonical_cookie::CookieHashSet* cookie_set) {
  return cookie_set->erase(add_cookie) > 0;
}

canonical_cookie::CookieHashSet* CannedBrowsingDataCookieHelper::GetCookiesFor(
    const GURL& first_party_origin) {
  OriginCookieSetMap::iterator it =
      origin_cookie_set_map_.find(first_party_origin);
  if (it == origin_cookie_set_map_.end()) {
    canonical_cookie::CookieHashSet* cookies =
        new canonical_cookie::CookieHashSet;
    origin_cookie_set_map_.insert(
        std::pair<GURL, canonical_cookie::CookieHashSet*>(first_party_origin,
                                                          cookies));
    return cookies;
  }
  return it->second;
}

void CannedBrowsingDataCookieHelper::AddCookie(
    const GURL& frame_url,
    const net::CanonicalCookie& cookie) {
  // Storing cookies in separate cookie sets per frame origin makes the
  // GetCookieCount method count a cookie multiple times if it is stored in
  // multiple sets.
  // E.g. let "example.com" be redirected to "www.example.com". A cookie set
  // with the cookie string "A=B; Domain=.example.com" would be sent to both
  // hosts. This means it would be stored in the separate cookie sets for both
  // hosts ("example.com", "www.example.com"). The method GetCookieCount would
  // count this cookie twice. To prevent this, we us a single global cookie
  // set as a work-around to store all added cookies. Per frame URL cookie
  // sets are currently not used. In the future they will be used for
  // collecting cookies per origin in redirect chains.
  // TODO(markusheintz): A) Change the GetCookiesCount method to prevent
  // counting cookies multiple times if they are stored in multiple cookie
  // sets.  B) Replace the GetCookieFor method call below with:
  // "GetCookiesFor(frame_url.GetOrigin());"
  CR_DEFINE_STATIC_LOCAL(const GURL, origin_cookie_url, (kGlobalCookieSetURL));
  canonical_cookie::CookieHashSet* cookie_set =
      GetCookiesFor(origin_cookie_url);
  DeleteMatchingCookie(cookie, cookie_set);
  cookie_set->insert(cookie);
}
