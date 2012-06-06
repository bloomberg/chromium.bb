// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mock_browsing_data_cookie_helper.h"

#include "base/logging.h"

MockBrowsingDataCookieHelper::MockBrowsingDataCookieHelper(
    net::URLRequestContextGetter* request_context_getter)
    : BrowsingDataCookieHelper(request_context_getter) {
}

MockBrowsingDataCookieHelper::~MockBrowsingDataCookieHelper() {
}

void MockBrowsingDataCookieHelper::StartFetching(
    const net::CookieMonster::GetCookieListCallback &callback) {
  callback_ = callback;
}

void MockBrowsingDataCookieHelper::DeleteCookie(
    const net::CookieMonster::CanonicalCookie& cookie) {
  std::string key = cookie.Name() + "=" + cookie.Value();
  CHECK(cookies_.find(key) != cookies_.end());
  cookies_[key] = false;
}

void MockBrowsingDataCookieHelper::AddCookieSamples(
    const GURL& url, const std::string& cookie_line) {
  typedef net::CookieList::const_iterator cookie_iterator;
  net::CookieMonster::ParsedCookie pc(cookie_line);
  scoped_ptr<net::CookieMonster::CanonicalCookie> cc;
  cc.reset(new net::CookieMonster::CanonicalCookie(url, pc));

  if (cc.get()) {
    for (cookie_iterator cookie = cookie_list_.begin();
        cookie != cookie_list_.end(); ++cookie) {
      if (cookie->Name() == cc->Name() &&
          cookie->Domain() == cc->Domain()&&
          cookie->Path() == cc->Path()) {
        return;
      }
    }
    cookie_list_.push_back(*cc);
    cookies_[cookie_line] = true;
  }
}

void MockBrowsingDataCookieHelper::Notify() {
  if (!callback_.is_null())
    callback_.Run(cookie_list_);
}

void MockBrowsingDataCookieHelper::Reset() {
  for (std::map<const std::string, bool>::iterator i = cookies_.begin();
       i != cookies_.end(); ++i)
    i->second = true;
}

bool MockBrowsingDataCookieHelper::AllDeleted() {
  for (std::map<const std::string, bool>::const_iterator i = cookies_.begin();
       i != cookies_.end(); ++i)
    if (i->second)
      return false;
  return true;
}
