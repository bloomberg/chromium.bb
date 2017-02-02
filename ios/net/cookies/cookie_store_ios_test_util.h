// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_COOKIE_STORE_IOS_TEST_UTIL_H_
#define IOS_NET_COOKIES_COOKIE_STORE_IOS_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "url/gurl.h"

namespace net {

class CanonicalCookie;

// Test net::CookieMonster::PersistentCookieStore allowing to control when the
// initialization completes.
class TestPersistentCookieStore
    : public net::CookieMonster::PersistentCookieStore {
 public:
  TestPersistentCookieStore();

  // Runs the completion callback with a "a=b" cookie.
  void RunLoadedCallback();

  bool flushed();

 private:
  // net::CookieMonster::PersistentCookieStore implementation:
  void Load(const LoadedCallback& loaded_callback) override;
  void LoadCookiesForKey(const std::string& key,
                         const LoadedCallback& loaded_callback) override;
  void AddCookie(const net::CanonicalCookie& cc) override;
  void UpdateCookieAccessTime(const net::CanonicalCookie& cc) override;
  void DeleteCookie(const net::CanonicalCookie& cc) override;
  void SetForceKeepSessionState() override;
  void Flush(const base::Closure& callback) override;

  ~TestPersistentCookieStore() override;

  const GURL kTestCookieURL;
  LoadedCallback loaded_callback_;
  bool flushed_;
};

// Helper callback to be passed to CookieStore::GetCookiesWithOptionsAsync().
class GetCookieCallback {
 public:
  GetCookieCallback();

  // Returns true if the callback has been run.
  bool did_run();

  // Returns the parameter of the callback.
  const std::string& cookie_line();

  void Run(const std::string& cookie_line);

 private:
  bool did_run_;
  std::string cookie_line_;
};

void RecordCookieChanges(std::vector<net::CanonicalCookie>* out_cookies,
                         std::vector<bool>* out_removes,
                         const net::CanonicalCookie& cookie,
                         net::CookieStore::ChangeCause cause);

// Sets a cookie.
void SetCookie(const std::string& cookie_line,
               const GURL& url,
               net::CookieStore* store);

// Clears the underlying NSHTTPCookieStorage.
void ClearCookies();

}  // namespace net

#endif  // IOS_NET_COOKIES_COOKIE_STORE_IOS_TEST_UTIL_H_
