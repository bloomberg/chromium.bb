// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_cookie_policy.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"

// ----------------------------------------------------------------------------

ChromeCookiePolicy::ChromeCookiePolicy(HostContentSettingsMap* map)
    : host_content_settings_map_(map),
      strict_third_party_blocking_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kBlockReadingThirdPartyCookies)) {}

ChromeCookiePolicy::~ChromeCookiePolicy() {}

int ChromeCookiePolicy::CanGetCookies(const GURL& url,
                                      const GURL& first_party) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (host_content_settings_map_->BlockThirdPartyCookies()) {
    net::StaticCookiePolicy policy(strict_third_party_blocking_ ?
        net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES :
        net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES);
    int rv = policy.CanGetCookies(url, first_party);
    DCHECK_NE(net::ERR_IO_PENDING, rv);
    if (rv != net::OK)
      return rv;
  }

  int policy = CheckPolicy(url);
  if (policy == net::OK_FOR_SESSION_ONLY)
    policy = net::OK;
  DCHECK_NE(net::ERR_IO_PENDING, policy);
  return policy;
}

int ChromeCookiePolicy::CanSetCookie(const GURL& url,
                                     const GURL& first_party,
                                     const std::string& cookie_line) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (host_content_settings_map_->BlockThirdPartyCookies()) {
    net::StaticCookiePolicy policy(strict_third_party_blocking_ ?
        net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES :
        net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES);
    int rv = policy.CanSetCookie(url, first_party, cookie_line);
    if (rv != net::OK)
      return rv;
  }

  int policy = CheckPolicy(url);
  DCHECK_NE(net::ERR_IO_PENDING, policy);
  return policy;
}

int ChromeCookiePolicy::CheckPolicy(const GURL& url) const {
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      url, CONTENT_SETTINGS_TYPE_COOKIES, "");
  if (setting == CONTENT_SETTING_BLOCK)
    return net::ERR_ACCESS_DENIED;
  if (setting == CONTENT_SETTING_ALLOW)
    return net::OK;
  if (setting == CONTENT_SETTING_SESSION_ONLY)
    return net::OK_FOR_SESSION_ONLY;
  NOTREACHED();
  return net::ERR_ACCESS_DENIED;
}
