// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_cookie_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

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

void BrowsingDataCookieHelper::CancelNotification() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  completion_callback_.Reset();
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
  if (!completion_callback_.is_null()) {
    completion_callback_.Run(cookies);
    completion_callback_.Reset();
  }
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
    : BrowsingDataCookieHelper(profile),
    profile_(profile) {
}

CannedBrowsingDataCookieHelper::~CannedBrowsingDataCookieHelper() {}

CannedBrowsingDataCookieHelper* CannedBrowsingDataCookieHelper::Clone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CannedBrowsingDataCookieHelper* clone =
      new CannedBrowsingDataCookieHelper(profile_);

  clone->cookie_list_ = cookie_list_;
  return clone;
}

void CannedBrowsingDataCookieHelper::AddReadCookies(
    const GURL& url,
    const net::CookieList& cookie_list) {
  typedef net::CookieList::const_iterator cookie_iterator;
  for (cookie_iterator add_cookie = cookie_list.begin();
       add_cookie != cookie_list.end(); ++add_cookie) {
    DeleteMetchingCookie(*add_cookie);
    cookie_list_.push_back(*add_cookie);
  }
}

void CannedBrowsingDataCookieHelper::AddChangedCookie(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options) {
  typedef net::CookieList::iterator cookie_iterator;

  net::CookieMonster::ParsedCookie pc(cookie_line);
  if (options.exclude_httponly() && pc.IsHttpOnly()) {
    // Return if a Javascript cookie illegally specified the HTTP only flag.
    return;
  }

  scoped_ptr<net::CookieMonster::CanonicalCookie> cc;
  // This fails to create a canonical cookie, if the normalized cookie domain
  // form cookie line and the url don't have the same domain+registry, or url
  // host isn't cookie domain or one of its subdomains.
  cc.reset(net::CookieMonster::CanonicalCookie::Create(url, pc));

  if (cc.get()) {
    DeleteMetchingCookie(*cc);
    cookie_list_.push_back(*cc);
  }
}

void CannedBrowsingDataCookieHelper::Reset() {
  cookie_list_.clear();
}

bool CannedBrowsingDataCookieHelper::empty() const {
  return cookie_list_.empty();
}

void CannedBrowsingDataCookieHelper::StartFetching(
    const net::CookieMonster::GetCookieListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback.is_null())
    callback.Run(cookie_list_);
}

void CannedBrowsingDataCookieHelper::CancelNotification() {}

bool CannedBrowsingDataCookieHelper::DeleteMetchingCookie(
    const net::CookieMonster::CanonicalCookie& add_cookie) {
  typedef net::CookieList::iterator cookie_iterator;
  for (cookie_iterator cookie = cookie_list_.begin();
      cookie != cookie_list_.end(); ++cookie) {
    if (cookie->Name() == add_cookie.Name() &&
        cookie->Domain() == add_cookie.Domain()&&
        cookie->Path() == add_cookie.Path()) {
      cookie_list_.erase(cookie);
      return true;
    }
  }
  return false;
}
