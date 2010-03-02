// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_profile_impl.h"

#include <map>

#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profile.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context.h"
#include "chrome/test/automation/automation_messages.h"

namespace AutomationRequestContext {

// A special Request context for automation. Substitute a few things
// like cookie store, proxy settings etc to handle them differently
// for automation.
class AutomationURLRequestContext : public ChromeURLRequestContext {
 public:
  AutomationURLRequestContext(ChromeURLRequestContext* original_context,
                              net::CookieStore* automation_cookie_store,
                              net::CookiePolicy* automation_cookie_policy)
      : ChromeURLRequestContext(original_context),
        // We must hold a reference to |original_context|, since many
        // of the dependencies that ChromeURLRequestContext(original_context)
        // copied are scoped to |original_context|.
        original_context_(original_context) {
    cookie_policy_ = automation_cookie_policy;
    cookie_store_ = automation_cookie_store;
  }

  virtual bool IsExternal() const {
    return true;
  }

 private:
  virtual ~AutomationURLRequestContext() {
    // Clear out members before calling base class dtor since we don't
    // own any of them.

    // Clear URLRequestContext members.
    host_resolver_ = NULL;
    proxy_service_ = NULL;
    http_transaction_factory_ = NULL;
    ftp_transaction_factory_ = NULL;
    cookie_store_ = NULL;
    transport_security_state_ = NULL;
  }

  scoped_refptr<ChromeURLRequestContext> original_context_;
  DISALLOW_COPY_AND_ASSIGN(AutomationURLRequestContext);
};

// CookieStore specialization to have automation specific
// behavior for cookies.
class AutomationCookieStore : public net::CookieStore {
 public:
  AutomationCookieStore(net::CookieStore* original_cookie_store,
                        AutomationResourceMessageFilter* automation_client,
                        int tab_handle)
      : original_cookie_store_(original_cookie_store),
        automation_client_(automation_client),
        tab_handle_(tab_handle) {
  }

  virtual ~AutomationCookieStore() {
    DLOG(INFO) << "In " << __FUNCTION__;
  }

  // CookieStore implementation.
  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const net::CookieOptions& options) {
    // The cookie_string_ is available only once, i.e. once it is read by
    // it is invalidated.
    cookie_string_ = cookie_line;
    return true;
  }

  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const net::CookieOptions& options) {
    return cookie_string_;
  }

  virtual void DeleteCookie(const GURL& url,
                            const std::string& cookie_name) {
    NOTREACHED() << "Should not get called for an automation profile";
  }

  virtual net::CookieMonster* GetCookieMonster() {
    NOTREACHED() << "Should not get called for an automation profile";
    return NULL;
  }

 protected:
  void SendIPCMessageOnIOThread(IPC::Message* m) {
    if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
      automation_client_->Send(m);
    } else {
      Task* task = NewRunnableMethod(this,
          &AutomationCookieStore::SendIPCMessageOnIOThread, m);
      ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, task);
    }
  }

  net::CookieStore* original_cookie_store_;
  scoped_refptr<AutomationResourceMessageFilter> automation_client_;
  int tab_handle_;
  std::string cookie_string_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomationCookieStore);
};

// CookiePolicy specialization for automation specific cookie policies.
class AutomationCookiePolicy : public net::CookiePolicy {
 public:
  AutomationCookiePolicy(AutomationResourceMessageFilter* automation_client,
                         int tab_handle, net::CookieStore* cookie_store)
      : automation_client_(automation_client),
        tab_handle_(tab_handle),
        cookie_store_(cookie_store) {}

  virtual int CanGetCookies(const GURL& url,
                            const GURL& first_party_for_cookies,
                            net::CompletionCallback* callback) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

    if (automation_client_.get()) {
      automation_client_->GetCookiesForUrl(tab_handle_, url, callback,
                                           cookie_store_.get());
      return net::ERR_IO_PENDING;
    }
    return net::ERR_ACCESS_DENIED;
  }

  virtual int CanSetCookie(const GURL& url,
                           const GURL& first_party_for_cookies,
                           const std::string& cookie_line,
                           net::CompletionCallback* callback) {
    if (automation_client_.get()) {
      automation_client_->Send(new AutomationMsg_SetCookieAsync(0,
          tab_handle_, url, cookie_line));
    }
    return net::ERR_ACCESS_DENIED;
  }

 private:
  scoped_refptr<AutomationResourceMessageFilter> automation_client_;
  int tab_handle_;
  scoped_refptr<net::CookieStore> cookie_store_;

  DISALLOW_COPY_AND_ASSIGN(AutomationCookiePolicy);
};

class Factory : public ChromeURLRequestContextFactory {
 public:
  Factory(ChromeURLRequestContextGetter* original_context_getter,
          Profile* profile,
          AutomationResourceMessageFilter* automation_client,
          int tab_handle)
      : ChromeURLRequestContextFactory(profile),
        original_context_getter_(original_context_getter),
        automation_client_(automation_client),
        tab_handle_(tab_handle) {
  }

  virtual ChromeURLRequestContext* Create() {
    ChromeURLRequestContext* original_context =
        original_context_getter_->GetIOContext();

    // Create an automation cookie store.
    scoped_refptr<net::CookieStore> automation_cookie_store =
        new AutomationCookieStore(original_context->cookie_store(),
                                  automation_client_,
                                  tab_handle_);

    // Create an automation cookie policy.
    AutomationCookiePolicy* automation_cookie_policy =
        new AutomationCookiePolicy(automation_client_,
                                   tab_handle_,
                                   automation_cookie_store);

    return new AutomationURLRequestContext(original_context,
                                           automation_cookie_store,
                                           automation_cookie_policy);
  }

 private:
  scoped_refptr<ChromeURLRequestContextGetter> original_context_getter_;
  scoped_refptr<AutomationResourceMessageFilter> automation_client_;
  int tab_handle_;
};

ChromeURLRequestContextGetter* CreateAutomationURLRequestContextForTab(
    int tab_handle,
    Profile* profile,
    AutomationResourceMessageFilter* automation_client) {
  ChromeURLRequestContextGetter* original_context =
      static_cast<ChromeURLRequestContextGetter*>(
          profile->GetRequestContext());

  ChromeURLRequestContextGetter* request_context =
      new ChromeURLRequestContextGetter(
          NULL,  // Don't register an observer on PrefService.
          new Factory(original_context, profile, automation_client,
                      tab_handle));
  return request_context;
}

}  // namespace AutomationRequestContext

