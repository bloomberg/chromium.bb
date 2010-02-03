// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_cookie_policy.h"

#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/host_content_settings_map.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"

// If we queue up more than this number of completions, then switch from ASK to
// BLOCK.  More than this number of requests at once seems like it could be a
// sign of trouble anyways.
static const size_t kMaxCompletionsPerHost = 10000;

ChromeCookiePolicy::ChromeCookiePolicy(HostContentSettingsMap* map)
    : host_content_settings_map_(map) {
}

ChromeCookiePolicy::~ChromeCookiePolicy() {
  DCHECK(host_completions_map_.empty());
}

int ChromeCookiePolicy::CanGetCookies(const GURL& url,
                                      const GURL& first_party,
                                      net::CompletionCallback* callback) {
  if (host_content_settings_map_->BlockThirdPartyCookies()) {
    net::StaticCookiePolicy policy(
        net::StaticCookiePolicy::BLOCK_THIRD_PARTY_COOKIES);
    int rv = policy.CanGetCookies(url, first_party, NULL);
    if (rv != net::OK)
      return rv;
  }

  const std::string& host = url.host();

  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      host, CONTENT_SETTINGS_TYPE_COOKIES);
  if (setting == CONTENT_SETTING_BLOCK)
    return net::ERR_ACCESS_DENIED;
  if (setting == CONTENT_SETTING_ALLOW)
    return net::OK;

  DCHECK(callback);

  // If we are currently prompting the user for a 'set-cookie' matching this
  // host, then we need to defer reading cookies.

  HostCompletionsMap::iterator it = host_completions_map_.find(host);
  if (it == host_completions_map_.end())
    return net::OK;

  if (it->second.size() >= kMaxCompletionsPerHost) {
    LOG(ERROR) << "Would exceed kMaxCompletionsPerHost";
    return net::ERR_ACCESS_DENIED;
  }

  it->second.push_back(Completion::ForGetCookies(callback));

  return net::ERR_IO_PENDING;
}

int ChromeCookiePolicy::CanSetCookie(const GURL& url,
                                     const GURL& first_party,
                                     const std::string& cookie_line,
                                     net::CompletionCallback* callback) {
  if (host_content_settings_map_->BlockThirdPartyCookies()) {
    net::StaticCookiePolicy policy(
        net::StaticCookiePolicy::BLOCK_THIRD_PARTY_COOKIES);
    int rv = policy.CanSetCookie(url, first_party, cookie_line, NULL);
    if (rv != net::OK)
      return rv;
  }

  const std::string& host = url.host();

  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      host, CONTENT_SETTINGS_TYPE_COOKIES);
  if (setting == CONTENT_SETTING_BLOCK)
    return net::ERR_ACCESS_DENIED;
  if (setting == CONTENT_SETTING_ALLOW)
    return net::OK;

  DCHECK(callback);

  // Else, ask the user...

  Completions& completions = host_completions_map_[host];

  if (completions.size() >= kMaxCompletionsPerHost) {
    LOG(ERROR) << "Would exceed kMaxCompletionsPerHost";
    return net::ERR_ACCESS_DENIED;
  }

  completions.push_back(Completion::ForSetCookie(callback));

  PromptForSetCookie(host, cookie_line);
  return net::ERR_IO_PENDING;
}

void ChromeCookiePolicy::PromptForSetCookie(const std::string &host,
                                            const std::string& cookie_line) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &ChromeCookiePolicy::PromptForSetCookie, host,
                          cookie_line));
    return;
  }

  // TODO(darin): Prompt user!
#if 0
  MessageBox(NULL,
             UTF8ToWide(cookie_line).c_str(),
             UTF8ToWide(host).c_str(),
             MB_OK);
#endif

  DidPromptForSetCookie(host, net::OK);
}

void ChromeCookiePolicy::DidPromptForSetCookie(const std::string &host,
                                               int result) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this, &ChromeCookiePolicy::DidPromptForSetCookie,
                          host, result));
    return;
  }

  // Notify all callbacks, starting with the first until we hit another that
  // is for a 'set-cookie'.
  HostCompletionsMap::iterator it = host_completions_map_.find(host);
  CHECK(it != host_completions_map_.end());

  Completions& completions = it->second;
  CHECK(!completions.empty() && completions[0].is_set_cookie_request());

  // Gather the list of callbacks to notify, and remove them from the
  // completions list before handing control to the callbacks (in case
  // they should call back into us to modify host_completions_map_).

  std::vector<net::CompletionCallback*> callbacks;
  callbacks.push_back(completions[0].callback());
  size_t i = 1;
  for (; i < completions.size(); ++i) {
    if (completions[i].is_set_cookie_request())
      break;
    callbacks.push_back(completions[i].callback());
  }
  completions.erase(completions.begin(), completions.begin() + i);

  if (completions.empty())
    host_completions_map_.erase(it);

  for (size_t j = 0; j < callbacks.size(); ++j)
    callbacks[j]->Run(result);
}
