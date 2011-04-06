// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_COOKIE_POLICY_H_
#define CHROME_BROWSER_NET_CHROME_COOKIE_POLICY_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_policy.h"

class HostContentSettingsMap;

// Implements CookiePolicy that uses HostContentSettingsMap and
// net::StaticCookiePolicy to decide if the cookie should be blocked.
class ChromeCookiePolicy : public net::CookiePolicy {
 public:
  explicit ChromeCookiePolicy(HostContentSettingsMap* map);
  virtual ~ChromeCookiePolicy();

  // CookiePolicy methods:
  virtual int CanGetCookies(const GURL& url, const GURL& first_party) const;
  virtual int CanSetCookie(const GURL& url,
                           const GURL& first_party,
                           const std::string& cookie_line) const;

 private:
  int CheckPolicy(const GURL& url) const;

  const scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  // True if blocking third-party cookies also applies to reading them.
  bool strict_third_party_blocking_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCookiePolicy);
};

#endif  // CHROME_BROWSER_NET_CHROME_COOKIE_POLICY_H_
