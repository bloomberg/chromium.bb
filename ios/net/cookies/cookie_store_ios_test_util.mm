// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/cookie_store_ios_test_util.h"

#import <Foundation/Foundation.h>

#include "base/run_loop.h"
#import "ios/net/cookies/cookie_store_ios.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void IgnoreBoolean(bool not_used) {}

}  // namespace

#pragma mark -
#pragma mark TestPersistentCookieStore

TestPersistentCookieStore::TestPersistentCookieStore()
    : kTestCookieURL("http://foo.google.com/bar"), flushed_(false) {}

TestPersistentCookieStore::~TestPersistentCookieStore() = default;

#pragma mark -
#pragma mark TestPersistentCookieStore methods

void TestPersistentCookieStore::RunLoadedCallback() {
  std::vector<std::unique_ptr<net::CanonicalCookie>> cookies;
  net::CookieOptions options;
  options.set_include_httponly();

  std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
      kTestCookieURL, "a=b", base::Time::Now(), options));
  cookies.push_back(std::move(cookie));

  std::unique_ptr<net::CanonicalCookie> bad_canonical_cookie(
      net::CanonicalCookie::Create(
          GURL("http://domain/"), "name", "\x81r\xe4\xbd\xa0\xe5\xa5\xbd",
          std::string(), "/path/",
          base::Time(),  // creation
          base::Time(),  // expires
          false,         // secure
          false,         // httponly
          net::CookieSameSite::DEFAULT_MODE, net::COOKIE_PRIORITY_DEFAULT));
  cookies.push_back(std::move(bad_canonical_cookie));
  loaded_callback_.Run(std::move(cookies));
}

bool TestPersistentCookieStore::flushed() {
  return flushed_;
}

#pragma mark -
#pragma mark Private methods

void TestPersistentCookieStore::Load(const LoadedCallback& loaded_callback) {
  loaded_callback_ = loaded_callback;
}

void TestPersistentCookieStore::LoadCookiesForKey(
    const std::string& key,
    const LoadedCallback& loaded_callback) {
  loaded_callback_ = loaded_callback;
}

void TestPersistentCookieStore::AddCookie(const net::CanonicalCookie& cc) {}

void TestPersistentCookieStore::UpdateCookieAccessTime(
    const net::CanonicalCookie& cc) {}

void TestPersistentCookieStore::DeleteCookie(const net::CanonicalCookie& cc) {}

void TestPersistentCookieStore::SetForceKeepSessionState() {}

void TestPersistentCookieStore::Flush(const base::Closure& callback) {
  flushed_ = true;
}

#pragma mark -
#pragma mark GetCookieCallback

GetCookieCallback::GetCookieCallback() : did_run_(false) {}

#pragma mark -
#pragma mark GetCookieCallback methods

bool GetCookieCallback::did_run() {
  return did_run_;
}

const std::string& GetCookieCallback::cookie_line() {
  return cookie_line_;
}

void GetCookieCallback::Run(const std::string& cookie_line) {
  ASSERT_FALSE(did_run_);
  did_run_ = true;
  cookie_line_ = cookie_line;
}

//------------------------------------------------------------------------------

void RecordCookieChanges(std::vector<net::CanonicalCookie>* out_cookies,
                         std::vector<bool>* out_removes,
                         const net::CanonicalCookie& cookie,
                         net::CookieStore::ChangeCause cause) {
  DCHECK(out_cookies);
  out_cookies->push_back(cookie);
  if (out_removes)
    out_removes->push_back(net::CookieStore::ChangeCauseIsDeletion(cause));
}

void SetCookie(const std::string& cookie_line,
               const GURL& url,
               net::CookieStore* store) {
  net::CookieOptions options;
  options.set_include_httponly();
  store->SetCookieWithOptionsAsync(url, cookie_line, options,
                                   base::Bind(&IgnoreBoolean));
  net::CookieStoreIOS::NotifySystemCookiesChanged();
  // Wait until the flush is posted.
  base::RunLoop().RunUntilIdle();
}

void ClearCookies() {
  NSHTTPCookieStorage* store = [NSHTTPCookieStorage sharedHTTPCookieStorage];
  [store setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways];
  NSArray* cookies = [store cookies];
  for (NSHTTPCookie* cookie in cookies)
    [store deleteCookie:cookie];
  EXPECT_EQ(0u, [[store cookies] count]);
}

}  // namespace net