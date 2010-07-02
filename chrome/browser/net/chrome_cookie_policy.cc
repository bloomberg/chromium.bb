// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_cookie_policy.h"

#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/cookie_prompt_modal_dialog_delegate.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/message_box_handler.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"

// If we queue up more than this number of completions, then switch from ASK to
// BLOCK.  More than this number of requests at once seems like it could be a
// sign of trouble anyways.
static const size_t kMaxCompletionsPerHost = 10000;

// ----------------------------------------------------------------------------

// ChromeCookiePolicy cannot just subclass the delegate interface because we
// may have several prompts pending.
class ChromeCookiePolicy::PromptDelegate
    : public CookiePromptModalDialogDelegate {
 public:
  PromptDelegate(ChromeCookiePolicy* cookie_policy, const std::string& host)
      : cookie_policy_(cookie_policy),
        host_(host) {
  }

  // CookiesPromptViewDelegate methods:
  virtual void AllowSiteData(bool session_expire);
  virtual void BlockSiteData();

 private:
  void NotifyDone(int policy);

  scoped_refptr<ChromeCookiePolicy> cookie_policy_;
  std::string host_;
};

void ChromeCookiePolicy::PromptDelegate::AllowSiteData(bool session_expire) {
  int policy = net::OK;
  if (session_expire)
    policy = net::OK_FOR_SESSION_ONLY;
  NotifyDone(policy);
}

void ChromeCookiePolicy::PromptDelegate::BlockSiteData() {
  NotifyDone(net::ERR_ACCESS_DENIED);
}

void ChromeCookiePolicy::PromptDelegate::NotifyDone(int policy) {
  cookie_policy_->DidPromptForSetCookie(host_, policy);
  delete this;
}

// ----------------------------------------------------------------------------

ChromeCookiePolicy::ChromeCookiePolicy(HostContentSettingsMap* map)
    : host_content_settings_map_(map) {
}

ChromeCookiePolicy::~ChromeCookiePolicy() {
  DCHECK(host_completions_map_.empty());
}

int ChromeCookiePolicy::CanGetCookies(const GURL& url,
                                      const GURL& first_party,
                                      net::CompletionCallback* callback) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  if (host_content_settings_map_->BlockThirdPartyCookies()) {
    net::StaticCookiePolicy policy(
        net::StaticCookiePolicy::BLOCK_THIRD_PARTY_COOKIES);
    int rv = policy.CanGetCookies(url, first_party, NULL);
    if (rv != net::OK)
      return rv;
  }

  int policy = CheckPolicy(url);
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
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  if (host_content_settings_map_->BlockThirdPartyCookies()) {
    net::StaticCookiePolicy policy(
        net::StaticCookiePolicy::BLOCK_THIRD_PARTY_COOKIES);
    int rv = policy.CanSetCookie(url, first_party, cookie_line, NULL);
    if (rv != net::OK)
      return rv;
  }

  int policy = CheckPolicy(url);
  if (policy != net::ERR_IO_PENDING)
    return policy;

  DCHECK(callback);

  // Else, ask the user...

  Completions& completions = host_completions_map_[url.host()];

  if (completions.size() >= kMaxCompletionsPerHost) {
    LOG(ERROR) << "Would exceed kMaxCompletionsPerHost";
    policy = net::ERR_ACCESS_DENIED;
  } else {
    completions.push_back(Completion::ForSetCookie(callback));
    policy = net::ERR_IO_PENDING;
  }

  PromptForSetCookie(url, cookie_line);
  return policy;
}

int ChromeCookiePolicy::CheckPolicy(const GURL& url) const {
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      url, CONTENT_SETTINGS_TYPE_COOKIES);
  if (setting == CONTENT_SETTING_BLOCK)
    return net::ERR_ACCESS_DENIED;
  if (setting == CONTENT_SETTING_ALLOW)
    return net::OK;
  if (setting == CONTENT_SETTING_SESSION_ONLY)
    return net::OK_FOR_SESSION_ONLY;
  return net::ERR_IO_PENDING;  // Need to prompt.
}

void ChromeCookiePolicy::PromptForSetCookie(const GURL& url,
                                            const std::string& cookie_line) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &ChromeCookiePolicy::PromptForSetCookie, url,
                          cookie_line));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  const std::string& host = url.host();

  // The policy may have changed (due to the "remember" option)
  int policy = CheckPolicy(url);
  if (policy != net::ERR_IO_PENDING) {
    DidPromptForSetCookie(host, policy);
    return;
  }

  // Show the prompt on top of the current tab.
  Browser* browser = BrowserList::GetLastActive();
  if (!browser || !browser->GetSelectedTabContents()) {
    DidPromptForSetCookie(host, net::ERR_ACCESS_DENIED);
    return;
  }

  RunCookiePrompt(browser->GetSelectedTabContents(),
                  host_content_settings_map_, url, cookie_line,
                  new PromptDelegate(this, host));
}

void ChromeCookiePolicy::DidPromptForSetCookie(const std::string& host,
                                               int policy) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this, &ChromeCookiePolicy::DidPromptForSetCookie,
                          host, policy));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

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
    callbacks[j]->Run(policy);
}
