// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl_request.h"

#include <memory>

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
  void OnBidirectionalStreamImplReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      BidirectionalStreamImpl* stream) override {}

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

  std::unique_ptr<HttpNetworkSession> session =
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

TEST_P(HttpStreamFactoryImplRequestTest, DelayMainJob) {
  SpdySessionDependencies session_deps(GetParam(),
                                       ProxyService::CreateDirect());

  std::unique_ptr<HttpNetworkSession> session =
      SpdySessionDependencies::SpdyCreateSession(&session_deps);

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  HttpStreamFactoryImpl* factory =
      static_cast<HttpStreamFactoryImpl*>(session->http_stream_factory());

  DoNothingRequestDelegate request_delegate;
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  HttpStreamFactoryImpl::Request request(
      request_info.url, factory, &request_delegate, NULL, BoundNetLog(),
      HttpStreamFactoryImpl::Request::HTTP_STREAM);

  HostPortPair server = HostPortPair::FromURL(request_info.url);
  GURL original_url = factory->ApplyHostMappingRules(request_info.url, &server);

  HttpStreamFactoryImpl::Job* job = new HttpStreamFactoryImpl::Job(
      factory, session.get(), request_info, DEFAULT_PRIORITY, SSLConfig(),
      SSLConfig(), server, original_url, NULL);
  request.AttachJob(job);
  EXPECT_EQ(DEFAULT_PRIORITY, job->priority());

  AlternativeService alternative_service(net::NPN_HTTP_2, server);
  HttpStreamFactoryImpl::Job* alternative_job = new HttpStreamFactoryImpl::Job(
      factory, session.get(), request_info, DEFAULT_PRIORITY, SSLConfig(),
      SSLConfig(), server, original_url, alternative_service, NULL);
  request.AttachJob(alternative_job);

  job->WaitFor(alternative_job);
  EXPECT_EQ(HttpStreamFactoryImpl::Job::STATE_NONE, job->next_state_);

  // Test |alternative_job| resuming the |job| after delay.
  int wait_time = 1;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(wait_time);
  job->Resume(alternative_job, delay);

  // Verify |job| has |wait_time_| and there is no |blocking_job_|
  EXPECT_EQ(delay, job->wait_time_);
  EXPECT_TRUE(!job->blocking_job_);

  // Start the |job| and verify |job|'s |wait_time_| is cleared.
  job->Start(&request);

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(wait_time + 1));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_NE(delay, job->wait_time_);
  EXPECT_TRUE(job->wait_time_.is_zero());
  EXPECT_EQ(HttpStreamFactoryImpl::Job::STATE_INIT_CONNECTION_COMPLETE,
            job->next_state_);
}

}  // namespace net
