// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/cookie_store_ios_persistent.h"

#import <Foundation/Foundation.h>

#include "ios/net/cookies/system_cookie_util.h"
#include "net/cookies/cookie_monster.h"

namespace net {

#pragma mark -
#pragma mark CookieStoreIOSPersistent

CookieStoreIOSPersistent::CookieStoreIOSPersistent(
    net::CookieMonster::PersistentCookieStore* persistent_store)
    : CookieStoreIOS(persistent_store,
                     [NSHTTPCookieStorage sharedHTTPCookieStorage]) {}

CookieStoreIOSPersistent::~CookieStoreIOSPersistent() {}

#pragma mark -
#pragma mark CookieStoreIOSPersistent methods

void CookieStoreIOSPersistent::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    const SetCookiesCallback& callback) {
  DCHECK(thread_checker().CalledOnValidThread());

  cookie_monster()->SetCookieWithOptionsAsync(url, cookie_line, options,
                                              WrapSetCallback(callback));
}

void CookieStoreIOSPersistent::SetCookieWithDetailsAsync(
    const GURL& url,
    const std::string& name,
    const std::string& value,
    const std::string& domain,
    const std::string& path,
    base::Time creation_time,
    base::Time expiration_time,
    base::Time last_access_time,
    bool secure,
    bool http_only,
    CookieSameSite same_site,
    CookiePriority priority,
    const SetCookiesCallback& callback) {
  DCHECK(thread_checker().CalledOnValidThread());

  cookie_monster()->SetCookieWithDetailsAsync(
      url, name, value, domain, path, creation_time, expiration_time,
      last_access_time, secure, http_only, same_site, priority,
      WrapSetCallback(callback));
}

void CookieStoreIOSPersistent::SetCanonicalCookieAsync(
    std::unique_ptr<CanonicalCookie> cookie,
    bool secure_source,
    bool modify_http_only,
    const SetCookiesCallback& callback) {
  DCHECK(thread_checker().CalledOnValidThread());

  cookie_monster()->SetCanonicalCookieAsync(std::move(cookie), secure_source,
                                            modify_http_only,
                                            WrapSetCallback(callback));
}

void CookieStoreIOSPersistent::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    const GetCookiesCallback& callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  cookie_monster()->GetCookiesWithOptionsAsync(url, options, callback);
}

void CookieStoreIOSPersistent::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    const GetCookieListCallback& callback) {
  DCHECK(thread_checker().CalledOnValidThread());

  cookie_monster()->GetCookieListWithOptionsAsync(url, options, callback);
}

void CookieStoreIOSPersistent::GetAllCookiesAsync(
    const GetCookieListCallback& callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  cookie_monster()->GetAllCookiesAsync(callback);
}

void CookieStoreIOSPersistent::DeleteCookieAsync(
    const GURL& url,
    const std::string& cookie_name,
    const base::Closure& callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  cookie_monster()->DeleteCookieAsync(url, cookie_name, WrapClosure(callback));
}

void CookieStoreIOSPersistent::DeleteCanonicalCookieAsync(
    const CanonicalCookie& cookie,
    const DeleteCallback& callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  cookie_monster()->DeleteCanonicalCookieAsync(cookie,
                                               WrapDeleteCallback(callback));
}

void CookieStoreIOSPersistent::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const DeleteCallback& callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  if (metrics_enabled())
    ResetCookieCountMetrics();

  cookie_monster()->DeleteAllCreatedBetweenAsync(delete_begin, delete_end,
                                                 WrapDeleteCallback(callback));
}

void CookieStoreIOSPersistent::DeleteAllCreatedBetweenWithPredicateAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const CookiePredicate& predicate,
    const DeleteCallback& callback) {
  DCHECK(thread_checker().CalledOnValidThread());

  if (metrics_enabled())
    ResetCookieCountMetrics();

  cookie_monster()->DeleteAllCreatedBetweenWithPredicateAsync(
      delete_begin, delete_end, predicate, WrapDeleteCallback(callback));
}

void CookieStoreIOSPersistent::DeleteSessionCookiesAsync(
    const DeleteCallback& callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  if (metrics_enabled())
    ResetCookieCountMetrics();

  cookie_monster()->DeleteSessionCookiesAsync(WrapDeleteCallback(callback));
}

#pragma mark -
#pragma mark Private methods

void CookieStoreIOSPersistent::WriteToCookieMonster(NSArray* system_cookies) {}

void CookieStoreIOSPersistent::OnSystemCookiesChanged() {}

}  // namespace net
