// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "components/grpc_support/include/bidirectional_stream_c.h"
#include "components/grpc_support/test/quic_test_server.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_server_properties_impl.h"
#include "net/url_request/url_request_test_util.h"

namespace grpc_support {
namespace {

// URLRequestContextGetter for BidirectionalStreamTest. This is used instead of
// net::TestURLRequestContextGetter because the URLRequestContext needs to be
// created on the test_io_thread_ for the test, and TestURLRequestContextGetter
// does not allow for lazy instantiation of the URLRequestContext if additional
// setup is required.
class BidirectionalStreamTestURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  BidirectionalStreamTestURLRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : task_runner_(task_runner) {}

  net::URLRequestContext* GetURLRequestContext() override {
    if (!request_context_.get()) {
      request_context_.reset(new net::TestURLRequestContext(
          true /* delay_initialization */));
      host_resolver_.reset(new net::MappedHostResolver(
          net::HostResolver::CreateDefaultResolver(nullptr)));
      host_resolver_->SetRulesFromString("MAP test.example.com 127.0.0.1,"
                                         "MAP notfound.example.com ~NOTFOUND");
      mock_cert_verifier_.reset(new net::MockCertVerifier());
      mock_cert_verifier_->set_default_result(net::OK);
      server_properties_.reset(new net::HttpServerPropertiesImpl());

      // Need to enable QUIC for the test server.
      auto params = base::MakeUnique<net::HttpNetworkSession::Params>();
      params->enable_quic = true;
      params->enable_http2 = true;
      net::AlternativeService alternative_service(net::AlternateProtocol::QUIC,
                                                  "", kTestServerPort);
      url::SchemeHostPort quic_hint_server("https", kTestServerDomain,
                                           kTestServerPort);
      server_properties_->SetAlternativeService(
          quic_hint_server, alternative_service, base::Time::Max());
      params->quic_host_whitelist.insert(kTestServerHost);

      request_context_->set_cert_verifier(mock_cert_verifier_.get());
      request_context_->set_host_resolver(host_resolver_.get());
      request_context_->set_http_server_properties(server_properties_.get());
      request_context_->set_http_network_session_params(std::move(params));

      request_context_->Init();
    }
    return request_context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return task_runner_;
  }

 private:
  ~BidirectionalStreamTestURLRequestContextGetter() override {}

  std::unique_ptr<net::HttpServerProperties> server_properties_;
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier_;
  std::unique_ptr<net::MappedHostResolver> host_resolver_;
  std::unique_ptr<net::TestURLRequestContext> request_context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BidirectionalStreamTestURLRequestContextGetter);
};

}  // namespace

stream_engine* GetTestStreamEngine() {
  static scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  static stream_engine engine;
  if (request_context_getter_.get() == nullptr) {
    static base::Thread* test_io_thread_ = new base::Thread(
        "grpc_support_test_io_thread");
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    DCHECK(test_io_thread_->StartWithOptions(options));

    request_context_getter_ =
        new BidirectionalStreamTestURLRequestContextGetter(
            test_io_thread_->task_runner());
    engine.obj = request_context_getter_.get();
  }
  return &engine;
}

}  // namespace grpc_support
