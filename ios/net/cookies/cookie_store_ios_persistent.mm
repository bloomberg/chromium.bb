// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/cookie_store_ios_persistent.h"

#import <Foundation/Foundation.h>

#include "base/memory/ptr_util.h"
#import "ios/net/cookies/ns_http_system_cookie_store.h"
#import "ios/net/cookies/system_cookie_util.h"
#include "net/cookies/cookie_monster.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

#pragma mark -
#pragma mark CookieStoreIOSPersistent

CookieStoreIOSPersistent::CookieStoreIOSPersistent(
    net::CookieMonster::PersistentCookieStore* persistent_store)
    : CookieStoreIOS(persistent_store,
                     base::MakeUnique<net::NSHTTPSystemCookieStore>()) {}

CookieStoreIOSPersistent::~CookieStoreIOSPersistent() {}

#pragma mark -
#pragma mark CookieStoreIOSPersistent methods

void CookieStoreIOSPersistent::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    SetCookiesCallback callback) {
  DCHECK(thread_checker().CalledOnValidThread());

  cookie_monster()->SetCookieWithOptionsAsync(
      url, cookie_line, options, WrapSetCallback(std::move(callback)));
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
    SetCookiesCallback callback) {
  DCHECK(thread_checker().CalledOnValidThread());

  cookie_monster()->SetCookieWithDetailsAsync(
      url, name, value, domain, path, creation_time, expiration_time,
      last_access_time, secure, http_only, same_site, priority,
      WrapSetCallback(std::move(callback)));
}

void CookieStoreIOSPersistent::SetCanonicalCookieAsync(
    std::unique_ptr<CanonicalCookie> cookie,
    bool secure_source,
    bool modify_http_only,
    SetCookiesCallback callback) {
  DCHECK(thread_checker().CalledOnValidThread());

  cookie_monster()->SetCanonicalCookieAsync(
      std::move(cookie), secure_source, modify_http_only,
      WrapSetCallback(std::move(callback)));
}

void CookieStoreIOSPersistent::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookiesCallback callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  cookie_monster()->GetCookiesWithOptionsAsync(url, options,
                                               std::move(callback));
}

void CookieStoreIOSPersistent::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookieListCallback callback) {
  DCHECK(thread_checker().CalledOnValidThread());

  cookie_monster()->GetCookieListWithOptionsAsync(url, options,
                                                  std::move(callback));
}

void CookieStoreIOSPersistent::GetAllCookiesAsync(
    GetCookieListCallback callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  cookie_monster()->GetAllCookiesAsync(std::move(callback));
}

void CookieStoreIOSPersistent::DeleteCookieAsync(const GURL& url,
                                                 const std::string& cookie_name,
                                                 base::OnceClosure callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  cookie_monster()->DeleteCookieAsync(url, cookie_name,
                                      WrapClosure(std::move(callback)));
}

void CookieStoreIOSPersistent::DeleteCanonicalCookieAsync(
    const CanonicalCookie& cookie,
    DeleteCallback callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  cookie_monster()->DeleteCanonicalCookieAsync(
      cookie, WrapDeleteCallback(std::move(callback)));
}

void CookieStoreIOSPersistent::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    DeleteCallback callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  if (metrics_enabled())
    ResetCookieCountMetrics();

  cookie_monster()->DeleteAllCreatedBetweenAsync(
      delete_begin, delete_end, WrapDeleteCallback(std::move(callback)));
}

void CookieStoreIOSPersistent::DeleteAllCreatedBetweenWithPredicateAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const CookiePredicate& predicate,
    DeleteCallback callback) {
  DCHECK(thread_checker().CalledOnValidThread());

  if (metrics_enabled())
    ResetCookieCountMetrics();

  cookie_monster()->DeleteAllCreatedBetweenWithPredicateAsync(
      delete_begin, delete_end, predicate,
      WrapDeleteCallback(std::move(callback)));
}

void CookieStoreIOSPersistent::DeleteSessionCookiesAsync(
    DeleteCallback callback) {
  DCHECK(thread_checker().CalledOnValidThread());
  if (metrics_enabled())
    ResetCookieCountMetrics();

  cookie_monster()->DeleteSessionCookiesAsync(
      WrapDeleteCallback(std::move(callback)));
}

#pragma mark -
#pragma mark Private methods

void CookieStoreIOSPersistent::WriteToCookieMonster(NSArray* system_cookies) {}

void CookieStoreIOSPersistent::OnSystemCookiesChanged() {}

}  // namespace net
