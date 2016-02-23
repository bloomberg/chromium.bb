// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_store.h"

#include "net/cookies/cookie_options.h"

namespace net {

CookieStore::CookieStore() {}

CookieStore::~CookieStore() {}

std::string CookieStore::BuildCookieLine(
    const std::vector<CanonicalCookie>& cookies) {
  std::string cookie_line;
  for (const auto& cookie : cookies) {
    if (!cookie_line.empty())
      cookie_line += "; ";
    // In Mozilla, if you set a cookie like "AAA", it will have an empty token
    // and a value of "AAA". When it sends the cookie back, it will send "AAA",
    // so we need to avoid sending "=AAA" for a blank token value.
    if (!cookie.Name().empty())
      cookie_line += cookie.Name() + "=";
    cookie_line += cookie.Value();
  }
  return cookie_line;
}

std::string CookieStore::BuildCookieLine(
    const std::vector<CanonicalCookie*>& cookies) {
  std::string cookie_line;
  for (const auto& cookie : cookies) {
    if (!cookie_line.empty())
      cookie_line += "; ";
    // In Mozilla, if you set a cookie like "AAA", it will have an empty token
    // and a value of "AAA". When it sends the cookie back, it will send "AAA",
    // so we need to avoid sending "=AAA" for a blank token value.
    if (!cookie->Name().empty())
      cookie_line += cookie->Name() + "=";
    cookie_line += cookie->Value();
  }
  return cookie_line;
}

void CookieStore::DeleteAllAsync(const DeleteCallback& callback) {
  DeleteAllCreatedBetweenAsync(base::Time(), base::Time::Max(), callback);
}

void CookieStore::SetForceKeepSessionState() {
  // By default, do nothing.
}

void CookieStore::GetAllCookiesForURLAsync(
    const GURL& url,
    const GetCookieListCallback& callback) {
  CookieOptions options;
  options.set_include_httponly();
  options.set_include_same_site();
  options.set_do_not_update_access_time();
  GetCookieListWithOptionsAsync(url, options, callback);
}

}  // namespace net
