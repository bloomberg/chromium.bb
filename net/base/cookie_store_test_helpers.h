// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COOKIE_STORE_TEST_HELPERS_H_
#define NET_BASE_COOKIE_STORE_TEST_HELPERS_H_
#pragma once

#include "net/base/cookie_monster.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class DelayedCookieMonster : public CookieStore {
 public:
  DelayedCookieMonster();

  // Call the asynchronous CookieMonster function, expect it to immediately
  // invoke the internal callback.
  // Post a delayed task to invoke the original callback with the results.

  virtual void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const CookieOptions& options,
      const CookieMonster::SetCookiesCallback& callback);

  virtual void GetCookiesWithOptionsAsync(
      const GURL& url, const CookieOptions& options,
      const CookieMonster::GetCookiesCallback& callback);

  virtual void GetCookiesWithInfoAsync(
      const GURL& url,
      const CookieOptions& options,
      const CookieMonster::GetCookieInfoCallback& callback);

  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const CookieOptions& options);

  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const CookieOptions& options);

  virtual void GetCookiesWithInfo(const GURL& url,
                                  const CookieOptions& options,
                                  std::string* cookie_line,
                                  std::vector<CookieInfo>* cookie_infos);

  virtual void DeleteCookie(const GURL& url,
                            const std::string& cookie_name);

  virtual void DeleteCookieAsync(const GURL& url,
                                 const std::string& cookie_name,
                                 const base::Closure& callback);

  virtual CookieMonster* GetCookieMonster();

 private:

  // Be called immediately from CookieMonster.

  void GetCookiesInternalCallback(
      std::string* cookie_line,
      std::vector<CookieStore::CookieInfo>* cookie_info);

  void SetCookiesInternalCallback(bool result);

  void GetCookiesWithOptionsInternalCallback(std::string cookie);

  // Invoke the original callbacks.

  void InvokeGetCookiesCallback(
      const CookieMonster::GetCookieInfoCallback& callback);

  void InvokeSetCookiesCallback(
      const CookieMonster::SetCookiesCallback& callback);

  void InvokeGetCookieStringCallback(
      const CookieMonster::GetCookiesCallback& callback);

  friend class base::RefCountedThreadSafe<DelayedCookieMonster>;
  virtual ~DelayedCookieMonster();

  scoped_refptr<CookieMonster> cookie_monster_;

  bool did_run_;
  bool result_;
  std::string cookie_;
  std::string cookie_line_;
  std::vector<CookieStore::CookieInfo> cookie_info_;
};

}  // namespace net

#endif  // NET_BASE_COOKIE_STORE_TEST_HELPERS_H_
