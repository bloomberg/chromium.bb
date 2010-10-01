// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cookie_store.h"

#include "net/base/cookie_options.h"

namespace net {

bool CookieStore::SetCookie(const GURL& url, const std::string& cookie_line) {
  return SetCookieWithOptions(url, cookie_line, CookieOptions());
}

std::string CookieStore::GetCookies(const GURL& url) {
  return GetCookiesWithOptions(url, CookieOptions());
}

void CookieStore::SetCookiesWithOptions(
    const GURL& url,
    const std::vector<std::string>& cookie_lines,
    const CookieOptions& options) {
  for (size_t i = 0; i < cookie_lines.size(); ++i)
    SetCookieWithOptions(url, cookie_lines[i], options);
}

void CookieStore::SetCookies(const GURL& url,
                             const std::vector<std::string>& cookie_lines) {
  SetCookiesWithOptions(url, cookie_lines, CookieOptions());
}

CookieStore::CookieStore() {}

CookieStore::~CookieStore() {}

}  // namespace net
