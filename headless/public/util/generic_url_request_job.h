// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_GENERIC_URL_REQUEST_JOB_H_
#define HEADLESS_PUBLIC_UTIL_GENERIC_URL_REQUEST_JOB_H_

#include <stddef.h>
#include <functional>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "headless/public/util/managed_dispatch_url_request_job.h"
#include "headless/public/util/url_fetcher.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
class IOBuffer;
}  // namespace net

namespace content {
class ResourceRequestInfo;
}  // namespace content

namespace headless {

class HeadlessBrowserContext;
class URLRequestDispatcher;

// Wrapper around net::URLRequest with helpers to access select metadata.
class Request {
 public:
  virtual uint64_t GetRequestId() const = 0;

  virtual const net::URLRequest* GetURLRequest() const = 0;

  // The frame from which the request came from.
  virtual int GetFrameTreeNodeId() const = 0;

  // The devtools agent host id for the page where the request came from.
  virtual std::string GetDevToolsAgentHostId() const = 0;

  // Gets the POST data, if any, from the net::URLRequest.
  virtual std::string GetPostData() const = 0;

  enum class ResourceType {
    MAIN_FRAME = 0,
    SUB_FRAME = 1,
    STYLESHEET = 2,
    SCRIPT = 3,
    IMAGE = 4,
    FONT_RESOURCE = 5,
    SUB_RESOURCE = 6,
    OBJECT = 7,
    MEDIA = 8,
    WORKER = 9,
    SHARED_WORKER = 10,
    PREFETCH = 11,
    FAVICON = 12,
    XHR = 13,
    PING = 14,
    SERVICE_WORKER = 15,
    CSP_REPORT = 16,
    PLUGIN_RESOURCE = 17,
    LAST_TYPE
  };

  virtual ResourceType GetResourceType() const = 0;

 protected:
  Request() {}
  virtual ~Request() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Request);
};

// Details of a pending request received by GenericURLRequestJob which must be
// either Allowed, Blocked, Modified or have it's response Mocked.
class PendingRequest {
 public:
  virtual const Request* GetRequest() const = 0;

  // Allows the request to proceed as normal.
  virtual void AllowRequest() = 0;

  // Causes the request to fail with the specified |error|.
  virtual void BlockRequest(net::Error error) = 0;

  // Allows the request to be completely re-written.
  virtual void ModifyRequest(
      const GURL& url,
      const std::string& method,
      const std::string& post_data,
      const net::HttpRequestHeaders& request_headers) = 0;

  struct MockResponseData {
    std::string response_data;
  };

  // Instead of fetching the request, |mock_response| is returned instead.
  virtual void MockResponse(
      std::unique_ptr<MockResponseData> mock_response) = 0;

 protected:
  PendingRequest() {}
  virtual ~PendingRequest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PendingRequest);
};

// Intended for use in a protocol handler, this ManagedDispatchURLRequestJob has
// the following features:
//
// 1. The delegate can extension observe / cancel and redirect requests
// 2. The delegate can optionally provide the results, otherwise the specifed
//    fetcher is invoked.
class HEADLESS_EXPORT GenericURLRequestJob
    : public ManagedDispatchURLRequestJob,
      public URLFetcher::ResultListener,
      public PendingRequest,
      public Request {
 public:
  class Delegate {
   public:
    // Notifies the delegate of an PendingRequest which must either be
    // allowed, blocked, modifed or it's response mocked. Called on an arbitrary
    // thread.
    virtual void OnPendingRequest(PendingRequest* pending_request) = 0;

    // Notifies the delegate of any fetch failure. Called on an arbitrary
    // thread.
    virtual void OnResourceLoadFailed(const Request* request,
                                      net::Error error) = 0;

    // Signals that a resource load has finished. Called on an arbitrary thread.
    virtual void OnResourceLoadComplete(
        const Request* request,
        const GURL& final_url,
        scoped_refptr<net::HttpResponseHeaders> response_headers,
        const char* body,
        size_t body_size) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // NOTE |url_request_dispatcher| and |delegate| must outlive the
  // GenericURLRequestJob.
  GenericURLRequestJob(net::URLRequest* request,
                       net::NetworkDelegate* network_delegate,
                       URLRequestDispatcher* url_request_dispatcher,
                       std::unique_ptr<URLFetcher> url_fetcher,
                       Delegate* delegate,
                       HeadlessBrowserContext* headless_browser_context);
  ~GenericURLRequestJob() override;

  // net::URLRequestJob implementation:
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;
  void Start() override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  bool GetMimeType(std::string* mime_type) const override;
  bool GetCharset(std::string* charset) override;
  void GetLoadTimingInfo(net::LoadTimingInfo* load_timing_info) const override;

  // URLFetcher::FetchResultListener implementation:
  void OnFetchStartError(net::Error error) override;
  void OnFetchComplete(const GURL& final_url,
                       scoped_refptr<net::HttpResponseHeaders> response_headers,
                       const char* body,
                       size_t body_size) override;

 protected:
  // Request implementation:
  uint64_t GetRequestId() const override;
  const net::URLRequest* GetURLRequest() const override;
  int GetFrameTreeNodeId() const override;
  std::string GetDevToolsAgentHostId() const override;
  std::string GetPostData() const override;
  ResourceType GetResourceType() const override;

  // PendingRequest implementation:
  const Request* GetRequest() const override;
  void AllowRequest() override;
  void BlockRequest(net::Error error) override;
  void ModifyRequest(const GURL& url,
                     const std::string& method,
                     const std::string& post_data,
                     const net::HttpRequestHeaders& request_headers) override;
  void MockResponse(std::unique_ptr<MockResponseData> mock_response) override;

 private:
  void PrepareCookies(const GURL& rewritten_url,
                      const std::string& method,
                      const url::Origin& site_for_cookies,
                      const base::Closure& done_callback);
  void OnCookiesAvailable(const GURL& rewritten_url,
                          const std::string& method,
                          const base::Closure& done_callback,
                          const net::CookieList& cookie_list);

  std::unique_ptr<URLFetcher> url_fetcher_;
  net::HttpRequestHeaders extra_request_headers_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
  std::unique_ptr<MockResponseData> mock_response_;
  Delegate* delegate_;          // Not owned.
  HeadlessBrowserContext* headless_browser_context_;           // Not owned.
  const content::ResourceRequestInfo* request_resource_info_;  // Not owned.
  const char* body_ = nullptr;  // Not owned.
  size_t body_size_ = 0;
  size_t read_offset_ = 0;
  base::TimeTicks response_time_;
  const uint64_t request_id_;
  static uint64_t next_request_id_;

  base::WeakPtrFactory<GenericURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GenericURLRequestJob);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_GENERIC_URL_REQUEST_JOB_H_
