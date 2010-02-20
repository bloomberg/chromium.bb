// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_COOKIE_POLICY_H_
#define CHROME_BROWSER_NET_CHROME_COOKIE_POLICY_H_

#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_policy.h"

class HostContentSettingsMap;

// Implements CookiePolicy that may delay queries to ask the user to decide.
//
// We will only prompt the user before setting a cookie.  We do not prompt the
// user before getting a cookie.  However, if we are in the process of
// prompting the user, then any requests to get cookies will be deferred.
// This is done so that cookie requests remain FIFO.
//
class ChromeCookiePolicy
    : public base::RefCountedThreadSafe<ChromeCookiePolicy>,
      public net::CookiePolicy {
 public:
  explicit ChromeCookiePolicy(HostContentSettingsMap* map);
  ~ChromeCookiePolicy();

  // CookiePolicy methods:
  virtual int CanGetCookies(const GURL& url, const GURL& first_party,
                            net::CompletionCallback* callback);
  virtual int CanSetCookie(const GURL& url, const GURL& first_party,
                           const std::string& cookie_line,
                           net::CompletionCallback* callback);

 private:
  class PromptDelegate;
  friend class PromptDelegate;

  class Completion {
   public:
    static Completion ForSetCookie(net::CompletionCallback* callback) {
      return Completion(true, callback);
    }

    static Completion ForGetCookies(net::CompletionCallback* callback) {
      return Completion(false, callback);
    }

    bool is_set_cookie_request() const { return is_set_cookie_request_; }
    net::CompletionCallback* callback() const { return callback_; }

   private:
    Completion(bool is_set_cookie_request, net::CompletionCallback* callback)
        : is_set_cookie_request_(is_set_cookie_request),
          callback_(callback) {
    }

    bool is_set_cookie_request_;
    net::CompletionCallback* callback_;
  };
  typedef std::vector<Completion> Completions;
  typedef std::map<std::string, Completions> HostCompletionsMap;

  int CheckPolicy(const GURL& url) const;
  void PromptForSetCookie(const GURL& url, const std::string& cookie_line);
  void DidPromptForSetCookie(const std::string& host, int result);

  // A map from hostname to callbacks awaiting a cookie policy response.
  // This map is only accessed on the IO thread.
  HostCompletionsMap host_completions_map_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
};

#endif  // CHROME_BROWSER_NET_CHROME_COOKIE_POLICY_H_
