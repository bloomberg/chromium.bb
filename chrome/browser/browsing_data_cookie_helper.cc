// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_cookie_helper.h"

#include "utility"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

BrowsingDataCookieHelper::BrowsingDataCookieHelper(Profile* profile)
  : is_fetching_(false),
    profile_(profile),
    request_context_getter_(profile->GetRequestContext()) {
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
    const net::CookieMonster::CanonicalCookie& cookie) {
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
  if (cookie_monster) {
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
    const net::CookieMonster::CanonicalCookie& cookie) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  scoped_refptr<net::CookieMonster> cookie_monster =
      request_context_getter_->GetURLRequestContext()->
      cookie_store()->GetCookieMonster();
  if (cookie_monster) {
    cookie_monster->DeleteCanonicalCookieAsync(
        cookie, net::CookieMonster::DeleteCookieCallback());
  }
}

CannedBrowsingDataCookieHelper::CannedBrowsingDataCookieHelper(
    Profile* profile)
    : BrowsingDataCookieHelper(profile) {
}

CannedBrowsingDataCookieHelper::~CannedBrowsingDataCookieHelper() {
  Reset();
}

CannedBrowsingDataCookieHelper* CannedBrowsingDataCookieHelper::Clone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CannedBrowsingDataCookieHelper* clone =
      new CannedBrowsingDataCookieHelper(profile());

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
  net::CookieMonster::ParsedCookie parsed_cookie(cookie_line);
  if (options.exclude_httponly() && parsed_cookie.IsHttpOnly()) {
    // Return if a Javascript cookie illegally specified the HTTP only flag.
    return;
  }

  // This fails to create a canonical cookie, if the normalized cookie domain
  // form cookie line and the url don't have the same domain+registry, or url
  // host isn't cookie domain or one of its subdomains.
  scoped_ptr<net::CookieMonster::CanonicalCookie> cookie(
      net::CookieMonster::CanonicalCookie::Create(url, parsed_cookie));
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

bool CannedBrowsingDataCookieHelper::DeleteMatchingCookie(
    const net::CookieMonster::CanonicalCookie& add_cookie,
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
    const net::CookieMonster::CanonicalCookie& cookie) {
  net::CookieList* cookie_list =
      GetCookiesFor(frame_url.GetOrigin());
  DeleteMatchingCookie(cookie, cookie_list);
  cookie_list->push_back(cookie);
}
