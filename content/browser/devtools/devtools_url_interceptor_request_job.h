// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_INTERCEPTOR_REQUEST_JOB_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_INTERCEPTOR_REQUEST_JOB_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_url_request_interceptor.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/resource_type.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace net {
class AuthChallengeInfo;
class UploadDataStream;
}

namespace content {

// A URLRequestJob that allows programmatic request blocking / modification or
// response mocking.  This class should only be accessed on the IO thread.
class DevToolsURLInterceptorRequestJob : public net::URLRequestJob,
                                         public net::URLRequest::Delegate {
 public:
  DevToolsURLInterceptorRequestJob(
      scoped_refptr<DevToolsURLRequestInterceptor::State>
          devtools_url_request_interceptor_state,
      const std::string& interception_id,
      net::URLRequest* original_request,
      net::NetworkDelegate* original_network_delegate,
      WebContents* web_contents,
      base::WeakPtr<protocol::NetworkHandler> network_handler,
      bool is_redirect,
      ResourceType resource_type);

  ~DevToolsURLInterceptorRequestJob() override;

  // net::URLRequestJob implementation:
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;
  void Start() override;
  void Kill() override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  int GetResponseCode() const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  bool GetMimeType(std::string* mime_type) const override;
  bool GetCharset(std::string* charset) override;
  void GetLoadTimingInfo(net::LoadTimingInfo* load_timing_info) const override;
  bool NeedsAuth() override;
  void GetAuthChallengeInfo(
      scoped_refptr<net::AuthChallengeInfo>* auth_info) override;

  void SetAuth(const net::AuthCredentials& credentials) override;
  void CancelAuth() override;

  // net::URLRequest::Delegate methods:
  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* auth_info) override;
  void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             const net::SSLInfo& ssl_info,
                             bool fatal) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;

  // Must be called on IO thread.
  void StopIntercepting();

  using ContinueInterceptedRequestCallback =
      protocol::Network::Backend::ContinueInterceptedRequestCallback;

  // Must be called only once per interception. Must be called on IO thread.
  void ContinueInterceptedRequest(
      std::unique_ptr<DevToolsURLRequestInterceptor::Modifications>
          modifications,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback);

  WebContents* web_contents() const { return web_contents_; }

 private:
  class SubRequest;

  // We keep a copy of the original request details to facilitate the
  // Network.modifyRequest command which could potentially change any of these
  // fields.
  struct RequestDetails {
    RequestDetails(const GURL& url,
                   const std::string& method,
                   std::unique_ptr<net::UploadDataStream> post_data,
                   const net::HttpRequestHeaders& extra_request_headers,
                   const net::RequestPriority& priority,
                   const net::URLRequestContext* url_request_context);
    ~RequestDetails();

    GURL url;
    std::string method;
    std::unique_ptr<net::UploadDataStream> post_data;
    net::HttpRequestHeaders extra_request_headers;
    net::RequestPriority priority;
    const net::URLRequestContext* url_request_context;
  };

  // If the request was either allowed or modified, a SubRequest will be used to
  // perform the fetch and the results proxied to the original request. This
  // gives us the flexibility to pretend redirects didn't happen if the user
  // chooses to mock the response.  Note this SubRequest is ignored by the
  // interceptor.
  class SubRequest {
   public:
    SubRequest(
        DevToolsURLInterceptorRequestJob::RequestDetails& request_details,
        DevToolsURLInterceptorRequestJob* devtools_interceptor_request_job,
        scoped_refptr<DevToolsURLRequestInterceptor::State>
            devtools_url_request_interceptor_state);
    ~SubRequest();

    void Cancel();

    net::URLRequest* request() const { return request_.get(); }

   private:
    std::unique_ptr<net::URLRequest> request_;

    DevToolsURLInterceptorRequestJob*
        devtools_interceptor_request_job_;  // NOT OWNED.

    scoped_refptr<DevToolsURLRequestInterceptor::State>
        devtools_url_request_interceptor_state_;
    bool fetch_in_progress_;
  };

  class MockResponseDetails {
   public:
    MockResponseDetails(std::string response_bytes,
                        base::TimeTicks response_time);

    MockResponseDetails(
        const scoped_refptr<net::HttpResponseHeaders>& response_headers,
        std::string response_bytes,
        size_t read_offset,
        base::TimeTicks response_time);

    ~MockResponseDetails();

    scoped_refptr<net::HttpResponseHeaders>& response_headers() {
      return response_headers_;
    }

    base::TimeTicks response_time() const { return response_time_; }

    int ReadRawData(net::IOBuffer* buf, int buf_size);

   private:
    scoped_refptr<net::HttpResponseHeaders> response_headers_;
    std::string response_bytes_;
    size_t read_offset_;
    base::TimeTicks response_time_;
  };

  // Retrieves the response headers from either the |sub_request_| or the
  // |mock_response_|.  In some cases (e.g. file access) this may be null.
  const net::HttpResponseHeaders* GetHttpResponseHeaders() const;

  void ProcessInterceptionRespose(
      std::unique_ptr<DevToolsURLRequestInterceptor::Modifications>
          modification);

  bool ProcessAuthRespose(
      std::unique_ptr<DevToolsURLRequestInterceptor::Modifications>
          modification);

  enum class WaitingForUserResponse {
    NOT_WAITING,
    WAITING_FOR_INTERCEPTION_RESPONSE,
    WAITING_FOR_AUTH_RESPONSE,
  };

  scoped_refptr<DevToolsURLRequestInterceptor::State>
      devtools_url_request_interceptor_state_;
  RequestDetails request_details_;
  std::unique_ptr<SubRequest> sub_request_;
  std::unique_ptr<MockResponseDetails> mock_response_details_;
  std::unique_ptr<net::RedirectInfo> redirect_;
  WaitingForUserResponse waiting_for_user_response_;
  bool intercepting_requests_;
  bool killed_;
  scoped_refptr<net::AuthChallengeInfo> auth_info_;

  const std::string interception_id_;
  WebContents* const web_contents_;
  const base::WeakPtr<protocol::NetworkHandler> network_handler_;
  const bool is_redirect_;
  const ResourceType resource_type_;
  base::WeakPtrFactory<DevToolsURLInterceptorRequestJob> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsURLInterceptorRequestJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_INTERCEPTOR_REQUEST_JOB_H_
