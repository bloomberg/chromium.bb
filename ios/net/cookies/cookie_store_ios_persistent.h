// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_COOKIE_STORE_IOS_PERSISTENT_H_
#define IOS_NET_COOKIES_COOKIE_STORE_IOS_PERSISTENT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#import "ios/net/cookies/cookie_store_ios.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "url/gurl.h"

@class NSHTTPCookieStorage;

namespace net {

// The CookieStoreIOSPersistent is an implementation of CookieStore relying on
// on backing CookieStore.
// CookieStoreIOSPersistent is not thread safe.
//
// All the changes are written back to the backing CookieStore.
// For synchronized CookieStore, please see CookieStoreIOS.
//
// TODO(crbug.com/686147): evaluate if inheritance is
// needed here.
class CookieStoreIOSPersistent : public CookieStoreIOS {
 public:
  // Creates a CookieStoreIOS with a default value of
  // |NSHTTPCookieStorage sharedCookieStorage| as the system's cookie store.
  explicit CookieStoreIOSPersistent(
      net::CookieMonster::PersistentCookieStore* persistent_store);

  ~CookieStoreIOSPersistent() override;

  // Inherited CookieStore methods.
  void SetCookieWithOptionsAsync(const GURL& url,
                                 const std::string& cookie_line,
                                 const net::CookieOptions& options,
                                 const SetCookiesCallback& callback) override;
  void SetCookieWithDetailsAsync(const GURL& url,
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
                                 const SetCookiesCallback& callback) override;
  void GetCookiesWithOptionsAsync(const GURL& url,
                                  const net::CookieOptions& options,
                                  const GetCookiesCallback& callback) override;
  void GetCookieListWithOptionsAsync(
      const GURL& url,
      const net::CookieOptions& options,
      const GetCookieListCallback& callback) override;
  void GetAllCookiesAsync(const GetCookieListCallback& callback) override;
  void DeleteCookieAsync(const GURL& url,
                         const std::string& cookie_name,
                         const base::Closure& callback) override;
  void DeleteCanonicalCookieAsync(const CanonicalCookie& cookie,
                                  const DeleteCallback& callback) override;
  void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                    const base::Time& delete_end,
                                    const DeleteCallback& callback) override;
  void DeleteAllCreatedBetweenWithPredicateAsync(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      const CookiePredicate& predicate,
      const DeleteCallback& callback) override;
  void DeleteSessionCookiesAsync(const DeleteCallback& callback) override;

 private:
  // No-op functions for this class.
  void WriteToCookieMonster(NSArray* system_cookies) override;
  void OnSystemCookiesChanged() override;

  DISALLOW_COPY_AND_ASSIGN(CookieStoreIOSPersistent);
};

}  // namespace net

#endif  // IOS_NET_COOKIES_COOKIE_STORE_IOS_PERSISTENT_H_
