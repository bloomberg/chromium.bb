// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_STORE_TEST_HELPERS_H_
#define NET_COOKIES_COOKIE_STORE_TEST_HELPERS_H_

#include "net/cookies/cookie_monster.h"

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class DelayedCookieMonster : public CookieStore {
 public:
  DelayedCookieMonster();

  // Call the asynchronous CookieMonster function, expect it to immediately
  // invoke the internal callback.
  // Post a delayed task to invoke the original callback with the results.

  void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const CookieOptions& options,
      const CookieMonster::SetCookiesCallback& callback) override;

  void GetCookiesWithOptionsAsync(
      const GURL& url,
      const CookieOptions& options,
      const CookieMonster::GetCookiesCallback& callback) override;

  void GetAllCookiesForURLAsync(const GURL& url,
                                const GetCookieListCallback& callback) override;

  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const CookieOptions& options);

  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const CookieOptions& options);

  virtual void DeleteCookie(const GURL& url,
                            const std::string& cookie_name);

  void DeleteCookieAsync(const GURL& url,
                         const std::string& cookie_name,
                         const base::Closure& callback) override;

  void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                    const base::Time& delete_end,
                                    const DeleteCallback& callback) override;

  void DeleteAllCreatedBetweenForHostAsync(
      const base::Time delete_begin,
      const base::Time delete_end,
      const GURL& url,
      const DeleteCallback& callback) override;

  void DeleteSessionCookiesAsync(const DeleteCallback&) override;

  CookieMonster* GetCookieMonster() override;

  scoped_ptr<CookieStore::CookieChangedSubscription>
  AddCallbackForCookie(const GURL& url, const std::string& name,
                       const CookieChangedCallback& callback) override;

 private:
  // Be called immediately from CookieMonster.

  void SetCookiesInternalCallback(bool result);

  void GetCookiesWithOptionsInternalCallback(const std::string& cookie);

  // Invoke the original callbacks.

  void InvokeSetCookiesCallback(
      const CookieMonster::SetCookiesCallback& callback);

  void InvokeGetCookieStringCallback(
      const CookieMonster::GetCookiesCallback& callback);

  friend class base::RefCountedThreadSafe<DelayedCookieMonster>;
  ~DelayedCookieMonster() override;

  scoped_refptr<CookieMonster> cookie_monster_;

  bool did_run_;
  bool result_;
  std::string cookie_;
  std::string cookie_line_;
};

class CookieURLHelper {
 public:
  explicit CookieURLHelper(const std::string& url_string);

  const std::string& domain() const { return domain_and_registry_; }
  std::string host() const { return url_.host(); }
  const GURL& url() const { return url_; }
  const GURL AppendPath(const std::string& path) const;

  // Return a new string with the following substitutions:
  // 1. "%R" -> Domain registry (i.e. "com")
  // 2. "%D" -> Domain + registry (i.e. "google.com")
  std::string Format(const std::string& format_string) const;

 private:
  const GURL url_;
  const std::string registry_;
  const std::string domain_and_registry_;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_STORE_TEST_HELPERS_H_
