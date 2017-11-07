// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_INTERCEPTOR_REQUEST_JOB_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_INTERCEPTOR_REQUEST_JOB_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
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
class DevToolsURLInterceptorRequestJob : public net::URLRequestJob {
 public:
  DevToolsURLInterceptorRequestJob(
      DevToolsURLRequestInterceptor* interceptor,
      const std::string& interception_id,
      net::URLRequest* original_request,
      net::NetworkDelegate* original_network_delegate,
      const base::UnguessableToken& devtools_token,
      const base::UnguessableToken& target_id,
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

  // Must be called on IO thread.
  void StopIntercepting();

  using ContinueInterceptedRequestCallback =
      protocol::Network::Backend::ContinueInterceptedRequestCallback;

  // Must be called only once per interception. Must be called on IO thread.
  void ContinueInterceptedRequest(
      std::unique_ptr<DevToolsURLRequestInterceptor::Modifications>
          modifications,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback);

  const base::UnguessableToken& target_id() const { return target_id_; }

 private:
  class SubRequest;
  class MockResponseDetails;

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

  // Callbacks from SubRequest.
  void OnSubRequestAuthRequired(net::AuthChallengeInfo* auth_info);
  void OnSubRequestRedirectReceived(const net::URLRequest& request,
                                    const net::RedirectInfo& redirectinfo,
                                    bool* defer_redirect);
  void OnSubRequestResponseStarted(const net::Error& net_error);

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
    WAITING_FOR_REQUEST_ACK,
    WAITING_FOR_AUTH_ACK,
  };

  DevToolsURLRequestInterceptor* const interceptor_;
  RequestDetails request_details_;
  std::unique_ptr<SubRequest> sub_request_;
  std::unique_ptr<MockResponseDetails> mock_response_details_;
  std::unique_ptr<net::RedirectInfo> redirect_;
  WaitingForUserResponse waiting_for_user_response_;
  bool intercepting_requests_;
  scoped_refptr<net::AuthChallengeInfo> auth_info_;

  const std::string interception_id_;
  base::UnguessableToken devtools_token_;
  // TODO(caseq): this really needs to be session id, not target id,
  // so that we can clean up pending intercept jobs for individual
  // session.
  base::UnguessableToken target_id_;
  const base::WeakPtr<protocol::NetworkHandler> network_handler_;
  const bool is_redirect_;
  const ResourceType resource_type_;
  base::WeakPtrFactory<DevToolsURLInterceptorRequestJob> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsURLInterceptorRequestJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_INTERCEPTOR_REQUEST_JOB_H_
