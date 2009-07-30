// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_profile_impl.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/test/automation/automation_messages.h"

// A special Request context for automation. Substitute a few things
// like cookie store, proxy settings etc to handle them differently
// for automation.
class AutomationURLRequestContext : public ChromeURLRequestContext {
 public:
  AutomationURLRequestContext(URLRequestContext* original_context,
                              net::CookieStore* automation_cookie_store)
        // All URLRequestContexts in chrome extend from ChromeURLRequestContext
      : ChromeURLRequestContext(static_cast<ChromeURLRequestContext*>(
            original_context)) {
    cookie_store_ = automation_cookie_store;
  }

  ~AutomationURLRequestContext() {
    // Clear out members before calling base class dtor since we don't
    // own any of them.

    // Clear URLRequestContext members.
    host_resolver_ = NULL;
    proxy_service_ = NULL;
    http_transaction_factory_ = NULL;
    ftp_transaction_factory_ = NULL;
    cookie_store_ = NULL;
    force_tls_state_ = NULL;

    // Clear ChromeURLRequestContext members.
    prefs_ = NULL;
    blacklist_ = NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomationURLRequestContext);
};

// CookieStore specialization to have automation specific
// behavior for cookies.
class AutomationCookieStore : public net::CookieStore {
 public:
  AutomationCookieStore(AutomationProfileImpl* profile,
                        net::CookieStore* original_cookie_store,
                        IPC::Message::Sender* automation_client)
      : profile_(profile),
        original_cookie_store_(original_cookie_store),
        automation_client_(automation_client) {
  }

  // CookieStore implementation.
  virtual bool SetCookie(const GURL& url, const std::string& cookie_line) {
    bool cookie_set = original_cookie_store_->SetCookie(url, cookie_line);
    if (cookie_set) {
      automation_client_->Send(new AutomationMsg_SetCookieAsync(0,
          profile_->tab_handle(), url, cookie_line));
    }
    return cookie_set;
  }
  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const net::CookieOptions& options) {
    return original_cookie_store_->SetCookieWithOptions(url, cookie_line,
                                                       options);
  }
  virtual bool SetCookieWithCreationTime(const GURL& url,
                                         const std::string& cookie_line,
                                         const base::Time& creation_time) {
    return original_cookie_store_->SetCookieWithCreationTime(url, cookie_line,
                                                            creation_time);
  }
  virtual bool SetCookieWithCreationTimeWithOptions(
                                         const GURL& url,
                                         const std::string& cookie_line,
                                         const base::Time& creation_time,
                                         const net::CookieOptions& options) {
    return original_cookie_store_->SetCookieWithCreationTimeWithOptions(url,
        cookie_line, creation_time, options);
  }
  virtual void SetCookies(const GURL& url,
                          const std::vector<std::string>& cookies) {
    original_cookie_store_->SetCookies(url, cookies);
  }
  virtual void SetCookiesWithOptions(const GURL& url,
                                     const std::vector<std::string>& cookies,
                                     const net::CookieOptions& options) {
    original_cookie_store_->SetCookiesWithOptions(url, cookies, options);
  }
  virtual std::string GetCookies(const GURL& url) {
    return original_cookie_store_->GetCookies(url);
  }
  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const net::CookieOptions& options) {
    return original_cookie_store_->GetCookiesWithOptions(url, options);
  }

 protected:
  AutomationProfileImpl* profile_;
  net::CookieStore* original_cookie_store_;
  IPC::Message::Sender* automation_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomationCookieStore);
};

void AutomationProfileImpl::Initialize(Profile* original_profile,
    IPC::Message::Sender* automation_client) {
  DCHECK(original_profile);
  original_profile_ = original_profile;

  URLRequestContext* original_context = original_profile_->GetRequestContext();
  net::CookieStore* original_cookie_store = original_context->cookie_store();
  alternate_cookie_store_.reset(new AutomationCookieStore(this,
                                                          original_cookie_store,
                                                          automation_client));
  alternate_reqeust_context_ = new AutomationURLRequestContext(
      original_context, alternate_cookie_store_.get());
}

