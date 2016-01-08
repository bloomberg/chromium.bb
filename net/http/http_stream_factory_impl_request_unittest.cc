// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl_request.h"

#include "base/memory/scoped_ptr.h"
#include "net/http/http_stream_factory_impl_job.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_failure_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class HttpStreamFactoryImplRequestTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<NextProto> {};

INSTANTIATE_TEST_CASE_P(NextProto,
                        HttpStreamFactoryImplRequestTest,
                        testing::Values(kProtoSPDY31,
                                        kProtoHTTP2));

namespace {

class DoNothingRequestDelegate : public HttpStreamRequest::Delegate {
 public:
  DoNothingRequestDelegate() {}

  ~DoNothingRequestDelegate() override {}

  // HttpStreamRequest::Delegate
  void OnStreamReady(const SSLConfig& used_ssl_config,
                     const ProxyInfo& used_proxy_info,
                     HttpStream* stream) override {}
  void OnBidirectionalStreamJobReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      BidirectionalStreamJob* stream_job) override {}

  void OnWebSocketHandshakeStreamReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      WebSocketHandshakeStreamBase* stream) override {}
  void OnStreamFailed(int status,
                      const SSLConfig& used_ssl_config,
                      SSLFailureState ssl_failure_state) override {}
  void OnCertificateError(int status,
                          const SSLConfig& used_ssl_config,
                          const SSLInfo& ssl_info) override {}
  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const SSLConfig& used_ssl_config,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override {}
  void OnNeedsClientAuth(const SSLConfig& used_ssl_config,
                         SSLCertRequestInfo* cert_info) override {}
  void OnHttpsProxyTunnelResponse(const HttpResponseInfo& response_info,
                                  const SSLConfig& used_ssl_config,
                                  const ProxyInfo& used_proxy_info,
                                  HttpStream* stream) override {}
  void OnQuicBroken() override {}
};

}  // namespace

// Make sure that Request passes on its priority updates to its jobs.
TEST_P(HttpStreamFactoryImplRequestTest, SetPriority) {
  SpdySessionDependencies session_deps(GetParam(),
                                       ProxyService::CreateDirect());

  scoped_ptr<HttpNetworkSession> session =
      SpdySessionDependencies::SpdyCreateSession(&session_deps);
  HttpStreamFactoryImpl* factory =
      static_cast<HttpStreamFactoryImpl*>(session->http_stream_factory());

  DoNothingRequestDelegate request_delegate;
  HttpStreamFactoryImpl::Request request(
      GURL(), factory, &request_delegate, NULL, BoundNetLog(),
      HttpStreamFactoryImpl::Request::HTTP_STREAM);

  HttpRequestInfo request_info;

  HostPortPair server = HostPortPair::FromURL(request_info.url);
  GURL original_url = factory->ApplyHostMappingRules(request_info.url, &server);

  HttpStreamFactoryImpl::Job* job = new HttpStreamFactoryImpl::Job(
      factory, session.get(), request_info, DEFAULT_PRIORITY, SSLConfig(),
      SSLConfig(), server, original_url, NULL);
  request.AttachJob(job);
  EXPECT_EQ(DEFAULT_PRIORITY, job->priority());

  request.SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, job->priority());

  // Make |job| the bound job.
  request.OnStreamFailed(job, ERR_FAILED, SSLConfig(), SSL_FAILURE_NONE);

  request.SetPriority(IDLE);
  EXPECT_EQ(IDLE, job->priority());
}

}  // namespace net
