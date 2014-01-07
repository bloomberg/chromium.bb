// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"

#include "utility"

#include "base/bind.h"
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
const char kGlobalCookieListURL[] = "chrome://cookielist";
}

BrowsingDataCookieHelper::BrowsingDataCookieHelper(
    net::URLRequestContextGetter* request_context_getter)
    : is_fetching_(false),
      request_context_getter_(request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

BrowsingDataCookieHelper::~BrowsingDataCookieHelper() {
}

void BrowsingDataCookieHelper::StartFetching(
    const base::Callback<void(const net::CookieList& cookies)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK(!callback.is_null());
  DCHECK(completion_callback_.is_null());
  is_fetching_ = true;
  completion_callback_ = callback;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BrowsingDataCookieHelper::FetchCookiesOnIOThread, this));
}

void BrowsingDataCookieHelper::DeleteCookie(
    const net::CanonicalCookie& cookie) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BrowsingDataCookieHelper::DeleteCookieOnIOThread,
                 this, cookie));
}

void BrowsingDataCookieHelper::FetchCookiesOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  scoped_refptr<net::CookieMonster> cookie_monster =
      request_context_getter_->GetURLRequestContext()->
      cookie_store()->GetCookieMonster();
  if (cookie_monster.get()) {
    cookie_monster->GetAllCookiesAsync(
        base::Bind(&BrowsingDataCookieHelper::OnFetchComplete, this));
  } else {
    OnFetchComplete(net::CookieList());
  }
}

void BrowsingDataCookieHelper::OnFetchComplete(const net::CookieList& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataCookieHelper::NotifyInUIThread, this, cookies));
}

void BrowsingDataCookieHelper::NotifyInUIThread(
    const net::CookieList& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(is_fetching_);
  is_fetching_ = false;
  completion_callback_.Run(cookies);
  completion_callback_.Reset();
}

void BrowsingDataCookieHelper::DeleteCookieOnIOThread(
    const net::CanonicalCookie& cookie) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  scoped_refptr<net::CookieMonster> cookie_monster =
      request_context_getter_->GetURLRequestContext()->
      cookie_store()->GetCookieMonster();
  if (cookie_monster.get()) {
    cookie_monster->DeleteCanonicalCookieAsync(
        cookie, net::CookieMonster::DeleteCookieCallback());
  }
}

CannedBrowsingDataCookieHelper::CannedBrowsingDataCookieHelper(
    net::URLRequestContextGetter* request_context_getter)
    : BrowsingDataCookieHelper(request_context_getter) {
}

CannedBrowsingDataCookieHelper::~CannedBrowsingDataCookieHelper() {
  Reset();
}

CannedBrowsingDataCookieHelper* CannedBrowsingDataCookieHelper::Clone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CannedBrowsingDataCookieHelper* clone =
      new CannedBrowsingDataCookieHelper(request_context_getter());

  for (OriginCookieListMap::iterator it = origin_cookie_list_map_.begin();
       it != origin_cookie_list_map_.end();
       ++it) {
    net::CookieList* cookies = clone->GetCookiesFor(it->first);
    cookies->insert(cookies->begin(), it->second->begin(), it->second->end());
  }
  return clone;
}

void CannedBrowsingDataCookieHelper::AddReadCookies(
    const GURL& frame_url,
    const GURL& url,
    const net::CookieList& cookie_list) {
  typedef net::CookieList::const_iterator cookie_iterator;
  for (cookie_iterator add_cookie = cookie_list.begin();
       add_cookie != cookie_list.end(); ++add_cookie) {
    AddCookie(frame_url, *add_cookie);
  }
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
  STLDeleteContainerPairSecondPointers(origin_cookie_list_map_.begin(),
                                       origin_cookie_list_map_.end());
  origin_cookie_list_map_.clear();
}

bool CannedBrowsingDataCookieHelper::empty() const {
  for (OriginCookieListMap::const_iterator it =
           origin_cookie_list_map_.begin();
       it != origin_cookie_list_map_.end();
       ++it) {
    if (!it->second->empty())
      return false;
  }
  return true;
}


size_t CannedBrowsingDataCookieHelper::GetCookieCount() const {
  size_t count = 0;
  for (OriginCookieListMap::const_iterator it = origin_cookie_list_map_.begin();
       it != origin_cookie_list_map_.end();
       ++it) {
    count += it->second->size();
  }
  return count;
}


void CannedBrowsingDataCookieHelper::StartFetching(
    const net::CookieMonster::GetCookieListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  net::CookieList cookie_list;
  for (OriginCookieListMap::iterator it = origin_cookie_list_map_.begin();
       it != origin_cookie_list_map_.end();
       ++it) {
    cookie_list.insert(cookie_list.begin(),
                       it->second->begin(),
                       it->second->end());
  }
  callback.Run(cookie_list);
}

void CannedBrowsingDataCookieHelper::DeleteCookie(
    const net::CanonicalCookie& cookie) {
  for (OriginCookieListMap::iterator it = origin_cookie_list_map_.begin();
       it != origin_cookie_list_map_.end();
       ++it) {
    DeleteMatchingCookie(cookie, it->second);
  }
  BrowsingDataCookieHelper::DeleteCookie(cookie);
}

bool CannedBrowsingDataCookieHelper::DeleteMatchingCookie(
    const net::CanonicalCookie& add_cookie,
    net::CookieList* cookie_list) {
  typedef net::CookieList::iterator cookie_iterator;
  for (cookie_iterator cookie = cookie_list->begin();
      cookie != cookie_list->end(); ++cookie) {
    if (cookie->Name() == add_cookie.Name() &&
        cookie->Domain() == add_cookie.Domain()&&
        cookie->Path() == add_cookie.Path()) {
      cookie_list->erase(cookie);
      return true;
    }
  }
  return false;
}

net::CookieList* CannedBrowsingDataCookieHelper::GetCookiesFor(
    const GURL& first_party_origin) {
  OriginCookieListMap::iterator it =
      origin_cookie_list_map_.find(first_party_origin);
  if (it == origin_cookie_list_map_.end()) {
    net::CookieList* cookies = new net::CookieList();
    origin_cookie_list_map_.insert(
        std::pair<GURL, net::CookieList*>(first_party_origin, cookies));
    return cookies;
  }
  return it->second;
}

void CannedBrowsingDataCookieHelper::AddCookie(
    const GURL& frame_url,
    const net::CanonicalCookie& cookie) {
  // Storing cookies in separate cookie lists per frame origin makes the
  // GetCookieCount method count a cookie multiple times if it is stored in
  // multiple lists.
  // E.g. let "example.com" be redirected to "www.example.com". A cookie set
  // with the cookie string "A=B; Domain=.example.com" would be sent to both
  // hosts. This means it would be stored in the separate cookie lists for both
  // hosts ("example.com", "www.example.com"). The method GetCookieCount would
  // count this cookie twice. To prevent this, we us a single global cookie
  // list as a work-around to store all added cookies. Per frame URL cookie
  // lists are currently not used. In the future they will be used for
  // collecting cookies per origin in redirect chains.
  // TODO(markusheintz): A) Change the GetCookiesCount method to prevent
  // counting cookies multiple times if they are stored in multiple cookie
  // lists.  B) Replace the GetCookieFor method call below with:
  // "GetCookiesFor(frame_url.GetOrigin());"
  net::CookieList* cookie_list =
      GetCookiesFor(GURL(kGlobalCookieListURL));
  DeleteMatchingCookie(cookie, cookie_list);
  cookie_list->push_back(cookie);
}
