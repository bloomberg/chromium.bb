// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_HTTP_BRIDGE_H_
#define CHROME_BROWSER_SYNC_GLUE_HTTP_BRIDGE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sync/engine/http_post_provider_factory.h"
#include "chrome/browser/sync/engine/http_post_provider_interface.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class MessageLoop;
class HttpBridgeTest;

namespace browser_sync {

// A bridge between the syncer and Chromium HTTP layers.
// Provides a way for the sync backend to use Chromium directly for HTTP
// requests rather than depending on a third party provider (e.g libcurl).
// This is a one-time use bridge. Create one for each request you want to make.
// It is RefCountedThreadSafe because it can PostTask to the io loop, and thus
// needs to stick around across context switches, etc.
class HttpBridge : public base::RefCountedThreadSafe<HttpBridge>,
                   public sync_api::HttpPostProviderInterface,
                   public URLFetcher::Delegate {
 public:
  // A request context used for HTTP requests bridged from the sync backend.
  // A bridged RequestContext has a dedicated in-memory cookie store and does
  // not use a cache. Thus the same type can be used for incognito mode.
  class RequestContext : public net::URLRequestContext {
   public:
    // |baseline_context| is used to obtain the accept-language,
    // accept-charsets, and proxy service information for bridged requests.
    // Typically |baseline_context| should be the net::URLRequestContext of the
    // currently active profile.
    explicit RequestContext(net::URLRequestContext* baseline_context);

    // Set the user agent for requests using this context. The default is
    // the browser's UA string.
    void set_user_agent(const std::string& ua) { user_agent_ = ua; }

    virtual const std::string& GetUserAgent(const GURL& url) const {
      // If the user agent is set explicitly return that, otherwise call the
      // base class method to return default value.
      return user_agent_.empty() ?
          net::URLRequestContext::GetUserAgent(url) : user_agent_;
    }

   private:
    // The destructor MUST be called on the IO thread.
    ~RequestContext();

    std::string user_agent_;
    net::URLRequestContext* baseline_context_;

    DISALLOW_COPY_AND_ASSIGN(RequestContext);
  };

  // Lazy-getter for RequestContext objects.
  class RequestContextGetter : public net::URLRequestContextGetter {
   public:
    explicit RequestContextGetter(
        net::URLRequestContextGetter* baseline_context_getter);

    void set_user_agent(const std::string& ua) { user_agent_ = ua; }
    bool is_user_agent_set() const { return !user_agent_.empty(); }

    // net::URLRequestContextGetter implementation.
    virtual net::URLRequestContext* GetURLRequestContext();
    virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const;

   private:
    ~RequestContextGetter() {}

    // User agent to apply to the net::URLRequestContext.
    std::string user_agent_;

    scoped_refptr<net::URLRequestContextGetter> baseline_context_getter_;

    // Lazily initialized by GetURLRequestContext().
    scoped_refptr<RequestContext> context_;

    DISALLOW_COPY_AND_ASSIGN(RequestContextGetter);
  };

  explicit HttpBridge(RequestContextGetter* context);

  // sync_api::HttpPostProvider implementation.
  virtual void SetUserAgent(const char* user_agent);
  virtual void SetExtraRequestHeaders(const char* headers);
  virtual void SetURL(const char* url, int port);
  virtual void SetPostPayload(const char* content_type, int content_length,
                              const char* content);
  virtual bool MakeSynchronousPost(int* os_error_code, int* response_code);
  virtual void Abort();

  // WARNING: these response content methods are used to extract plain old data
  // and not null terminated strings, so you should make sure you have read
  // GetResponseContentLength() characters when using GetResponseContent. e.g
  // string r(b->GetResponseContent(), b->GetResponseContentLength()).
  virtual int GetResponseContentLength() const;
  virtual const char* GetResponseContent() const;
  virtual const std::string GetResponseHeaderValue(
      const std::string& name) const;

  // URLFetcher::Delegate implementation.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

#if defined(UNIT_TEST)
  net::URLRequestContextGetter* GetRequestContextGetter() const {
    return context_getter_for_request_;
  }
#endif

 protected:
  friend class base::RefCountedThreadSafe<HttpBridge>;

  virtual ~HttpBridge();

  // Protected virtual so the unit test can override to shunt network requests.
  virtual void MakeAsynchronousPost();

 private:
  friend class ::HttpBridgeTest;

  // Called on the IO loop to issue the network request. The extra level
  // of indirection is so that the unit test can override this behavior but we
  // still have a function to statically pass to PostTask.
  void CallMakeAsynchronousPost() { MakeAsynchronousPost(); }

  // Gets a customized net::URLRequestContext for bridged requests. See
  // RequestContext definition for details.
  scoped_refptr<RequestContextGetter> context_getter_for_request_;

  // The message loop of the thread we were created on. This is the thread that
  // will block on MakeSynchronousPost while the IO thread fetches data from
  // the network.
  // This should be the main syncer thread (SyncerThread) which is what blocks
  // on network IO through curl_easy_perform.
  MessageLoop* const created_on_loop_;

  // The URL to POST to.
  GURL url_for_request_;

  // POST payload information.
  std::string content_type_;
  std::string request_content_;
  std::string extra_headers_;

  // A waitable event we use to provide blocking semantics to
  // MakeSynchronousPost. We block created_on_loop_ while the IO loop fetches
  // network request.
  base::WaitableEvent http_post_completed_;

  struct URLFetchState {
    URLFetchState();
    ~URLFetchState();
    // Our hook into the network layer is a URLFetcher. USED ONLY ON THE IO
    // LOOP, so we can block created_on_loop_ while the fetch is in progress.
    // NOTE: This is not a scoped_ptr for a reason. It must be deleted on the
    // same thread that created it, which isn't the same thread |this| gets
    // deleted on. We must manually delete url_poster_ on the IO loop.
    URLFetcher* url_poster;

    // Used to support 'Abort' functionality.
    bool aborted;

    // Cached response data.
    bool request_completed;
    bool request_succeeded;
    int http_response_code;
    int os_error_code;
    std::string response_content;
    scoped_refptr<net::HttpResponseHeaders> response_headers;
  };

  // This lock synchronizes use of state involved in the flow to fetch a URL
  // using URLFetcher.  Because we can Abort() from any thread, for example,
  // this flow needs to be synchronized to gracefully clean up URLFetcher and
  // return appropriate values in os_error_code.
  mutable base::Lock fetch_state_lock_;
  URLFetchState fetch_state_;

  DISALLOW_COPY_AND_ASSIGN(HttpBridge);
};

class HttpBridgeFactory : public sync_api::HttpPostProviderFactory {
 public:
  explicit HttpBridgeFactory(
      net::URLRequestContextGetter* baseline_context_getter);
  virtual ~HttpBridgeFactory();

  // sync_api::HttpPostProviderFactory:
  virtual sync_api::HttpPostProviderInterface* Create() OVERRIDE;
  virtual void Destroy(sync_api::HttpPostProviderInterface* http) OVERRIDE;

 private:
  // This request context is built on top of the baseline context and shares
  // common components.
  HttpBridge::RequestContextGetter* GetRequestContextGetter();

  scoped_refptr<HttpBridge::RequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(HttpBridgeFactory);
};

}  //  namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_HTTP_BRIDGE_H_
