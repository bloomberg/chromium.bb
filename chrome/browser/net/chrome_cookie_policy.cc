// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_cookie_policy.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"

// If we queue up more than this number of completions, then switch from ASK to
// BLOCK.  More than this number of requests at once seems like it could be a
// sign of trouble anyways.
static const size_t kMaxCompletionsPerHost = 10000;

// ----------------------------------------------------------------------------

ChromeCookiePolicy::ChromeCookiePolicy(HostContentSettingsMap* map)
    : host_content_settings_map_(map) {
  strict_third_party_blocking_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kBlockReadingThirdPartyCookies);
}

ChromeCookiePolicy::~ChromeCookiePolicy() {
  DCHECK(host_completions_map_.empty());
}

int ChromeCookiePolicy::CanGetCookies(const GURL& url,
                                      const GURL& first_party,
                                      net::CompletionCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (host_content_settings_map_->BlockThirdPartyCookies()) {
    net::StaticCookiePolicy policy(strict_third_party_blocking_ ?
        net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES :
        net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES);
    int rv = policy.CanGetCookies(url, first_party, NULL);
    if (rv != net::OK)
      return rv;
  }

  int policy = CheckPolicy(url);
  if (policy == net::OK_FOR_SESSION_ONLY)
    policy = net::OK;
  if (policy != net::ERR_IO_PENDING)
    return policy;

  DCHECK(callback);

  // If we are currently prompting the user for a 'set-cookie' matching this
  // host, then we need to defer reading cookies.
  HostCompletionsMap::iterator it = host_completions_map_.find(url.host());
  if (it == host_completions_map_.end()) {
    policy = net::OK;
  } else if (it->second.size() >= kMaxCompletionsPerHost) {
    LOG(ERROR) << "Would exceed kMaxCompletionsPerHost";
    policy = net::ERR_ACCESS_DENIED;
  } else {
    it->second.push_back(Completion::ForGetCookies(callback));
    policy = net::ERR_IO_PENDING;
  }
  return policy;
}

int ChromeCookiePolicy::CanSetCookie(const GURL& url,
                                     const GURL& first_party,
                                     const std::string& cookie_line,
                                     net::CompletionCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (host_content_settings_map_->BlockThirdPartyCookies()) {
    net::StaticCookiePolicy policy(strict_third_party_blocking_ ?
        net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES :
        net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES);
    int rv = policy.CanSetCookie(url, first_party, cookie_line, NULL);
    if (rv != net::OK)
      return rv;
  }

  int policy = CheckPolicy(url);
  if (policy != net::ERR_IO_PENDING)
    return policy;

  DCHECK(callback);

  Completions& completions = host_completions_map_[url.host()];
  if (completions.size() >= kMaxCompletionsPerHost) {
    LOG(ERROR) << "Would exceed kMaxCompletionsPerHost";
    policy = net::ERR_ACCESS_DENIED;
  } else {
    completions.push_back(Completion::ForSetCookie(callback));
    policy = net::ERR_IO_PENDING;
  }

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
  return net::ERR_IO_PENDING;  // Need to prompt.
}

