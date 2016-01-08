// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/quic_server.h"
#include "net/tools/quic/test_tools/quic_in_memory_cache_peer.h"
#include "net/tools/quic/test_tools/server_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::StringPiece;

namespace net {

using tools::QuicInMemoryCache;
using tools::QuicServer;
using tools::test::QuicInMemoryCachePeer;
using tools::test::ServerThread;

namespace test {

namespace {

const char kResponseBody[] = "some arbitrary response body";

// Factory for creating HttpTransactions, used by TestTransactionConsumer.
class TestTransactionFactory : public HttpTransactionFactory {
 public:
  explicit TestTransactionFactory(const HttpNetworkSession::Params& params)
      : session_(new HttpNetworkSession(params)) {}

  ~TestTransactionFactory() override {}

  // HttpTransactionFactory methods
  int CreateTransaction(RequestPriority priority,
                        scoped_ptr<HttpTransaction>* trans) override {
    trans->reset(new HttpNetworkTransaction(priority, session_.get()));
    return OK;
  }

  HttpCache* GetCache() override { return nullptr; }

  HttpNetworkSession* GetSession() override { return session_.get(); };

 private:
  scoped_ptr<HttpNetworkSession> session_;
};

}  // namespace

class QuicEndToEndTest : public PlatformTest {
 protected:
  QuicEndToEndTest()
      : host_resolver_impl_(CreateResolverImpl()),
        host_resolver_(std::move(host_resolver_impl_)),
        cert_transparency_verifier_(new MultiLogCTVerifier()),
        ssl_config_service_(new SSLConfigServiceDefaults),
        proxy_service_(ProxyService::CreateDirect()),
        auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(&host_resolver_)),
        strike_register_no_startup_period_(false) {
    request_.method = "GET";
    request_.url = GURL("https://test.example.com/");
    request_.load_flags = 0;

    params_.enable_quic = true;
    params_.quic_clock = nullptr;
    params_.quic_random = nullptr;
    params_.host_resolver = &host_resolver_;
    params_.cert_verifier = &cert_verifier_;
    params_.transport_security_state = &transport_security_state_;
    params_.cert_transparency_verifier = cert_transparency_verifier_.get();
    params_.proxy_service = proxy_service_.get();
    params_.ssl_config_service = ssl_config_service_.get();
    params_.http_auth_handler_factory = auth_handler_factory_.get();
    params_.http_server_properties = http_server_properties.GetWeakPtr();

    net::CertVerifyResult verify_result;
    verify_result.verified_cert = ImportCertFromFile(
        GetTestCertsDirectory(), "quic_test.example.com.crt");
    cert_verifier_.AddResultForCertAndHost(verify_result.verified_cert.get(),
                                           "test.example.com", verify_result,
                                           OK);
    verify_result.verified_cert = ImportCertFromFile(
        GetTestCertsDirectory(), "quic_test_ecc.example.com.crt");
    cert_verifier_.AddResultForCertAndHost(verify_result.verified_cert.get(),
                                           "test.example.com", verify_result,
                                           OK);
  }

  // Creates a mock host resolver in which test.example.com
  // resolves to localhost.
  static MockHostResolver* CreateResolverImpl() {
    MockHostResolver* resolver = new MockHostResolver();
    resolver->rules()->AddRule("test.example.com", "127.0.0.1");
    return resolver;
  }

  void SetUp() override {
    QuicInMemoryCachePeer::ResetForTests();
    StartServer();

    // Use a mapped host resolver so that request for test.example.com (port 80)
    // reach the server running on localhost.
    std::string map_rule = "MAP test.example.com test.example.com:" +
                           base::IntToString(server_thread_->GetPort());
    EXPECT_TRUE(host_resolver_.AddRuleFromString(map_rule));

    // To simplify the test, and avoid the race with the HTTP request, we force
    // QUIC for these requests.
    params_.origin_to_force_quic_on =
        HostPortPair::FromString("test.example.com:443");

    transaction_factory_.reset(new TestTransactionFactory(params_));
  }

  void TearDown() override {
    StopServer();
    QuicInMemoryCachePeer::ResetForTests();
  }

  // Starts the QUIC server listening on a random port.
  void StartServer() {
    IPAddressNumber ip;
    CHECK(ParseIPLiteralToNumber("127.0.0.1", &ip));
    server_address_ = IPEndPoint(ip, 0);
    server_config_.SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    server_config_.SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    QuicServer* server =
        new QuicServer(CryptoTestUtils::ProofSourceForTesting(), server_config_,
                       QuicSupportedVersions());
    server_thread_.reset(new ServerThread(server, server_address_,
                                          strike_register_no_startup_period_));
    server_thread_->Initialize();
    server_address_ =
        IPEndPoint(server_address_.address(), server_thread_->GetPort());
    server_thread_->Start();
    server_started_ = true;
  }

  // Stops the QUIC server.
  void StopServer() {
    if (!server_started_) {
      return;
    }
    if (server_thread_.get()) {
      server_thread_->Quit();
      server_thread_->Join();
    }
  }

  // Adds an entry to the cache used by the QUIC server to serve
  // responses.
  void AddToCache(StringPiece path,
                  int response_code,
                  StringPiece response_detail,
                  StringPiece body) {
    QuicInMemoryCache::GetInstance()->AddSimpleResponse(
        "test.example.com", path, response_code, body);
  }

  // Populates |request_body_| with |length_| ASCII bytes.
  void GenerateBody(size_t length) {
    request_body_.clear();
    request_body_.reserve(length);
    for (size_t i = 0; i < length; ++i) {
      request_body_.append(1, static_cast<char>(32 + i % (126 - 32)));
    }
  }

  // Initializes |request_| for a post of |length| bytes.
  void InitializePostRequest(size_t length) {
    GenerateBody(length);
    std::vector<scoped_ptr<UploadElementReader>> element_readers;
    element_readers.push_back(make_scoped_ptr(new UploadBytesElementReader(
        request_body_.data(), request_body_.length())));
    upload_data_stream_.reset(
        new ElementsUploadDataStream(std::move(element_readers), 0));
    request_.method = "POST";
    request_.url = GURL("https://test.example.com/");
    request_.upload_data_stream = upload_data_stream_.get();
    ASSERT_EQ(OK, request_.upload_data_stream->Init(CompletionCallback()));
  }

  // Checks that |consumer| completed and received |status_line| and |body|.
  void CheckResponse(const TestTransactionConsumer& consumer,
                     const std::string& status_line,
                     const std::string& body) {
    ASSERT_TRUE(consumer.is_done());
    ASSERT_EQ(OK, consumer.error());
    EXPECT_EQ(status_line, consumer.response_info()->headers->GetStatusLine());
    EXPECT_EQ(body, consumer.content());
  }

  scoped_ptr<MockHostResolver> host_resolver_impl_;
  MappedHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  scoped_ptr<CTVerifier> cert_transparency_verifier_;
  scoped_refptr<SSLConfigServiceDefaults> ssl_config_service_;
  scoped_ptr<ProxyService> proxy_service_;
  scoped_ptr<HttpAuthHandlerFactory> auth_handler_factory_;
  HttpServerPropertiesImpl http_server_properties;
  HttpNetworkSession::Params params_;
  scoped_ptr<TestTransactionFactory> transaction_factory_;
  HttpRequestInfo request_;
  std::string request_body_;
  scoped_ptr<UploadDataStream> upload_data_stream_;
  scoped_ptr<ServerThread> server_thread_;
  IPEndPoint server_address_;
  std::string server_hostname_;
  QuicConfig server_config_;
  bool server_started_;
  bool strike_register_no_startup_period_;
};

TEST_F(QuicEndToEndTest, LargeGetWithNoPacketLoss) {
  std::string response(10 * 1024, 'x');

  AddToCache(request_.url.PathForRequest(), 200, "OK", response);

  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, BoundNetLog());

  // Will terminate when the last consumer completes.
  base::MessageLoop::current()->Run();

  CheckResponse(consumer, "HTTP/1.1 200", response);
}

// crbug.com/559173
#if defined(THREAD_SANITIZER)
#define MAYBE_LargePostWithNoPacketLoss DISABLED_LargePostWithNoPacketLoss
#else
#define MAYBE_LargePostWithNoPacketLoss LargePostWithNoPacketLoss
#endif
TEST_F(QuicEndToEndTest, MAYBE_LargePostWithNoPacketLoss) {
  InitializePostRequest(1024 * 1024);

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, BoundNetLog());

  // Will terminate when the last consumer completes.
  base::MessageLoop::current()->Run();

  CheckResponse(consumer, "HTTP/1.1 200", kResponseBody);
}

// crbug.com/559173
#if defined(THREAD_SANITIZER)
#define MAYBE_LargePostWithPacketLoss DISABLED_LargePostWithPacketLoss
#else
#define MAYBE_LargePostWithPacketLoss LargePostWithPacketLoss
#endif
TEST_F(QuicEndToEndTest, MAYBE_LargePostWithPacketLoss) {
  // FLAGS_fake_packet_loss_percentage = 30;
  InitializePostRequest(1024 * 1024);

  const char kResponseBody[] = "some really big response body";
  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, BoundNetLog());

  // Will terminate when the last consumer completes.
  base::MessageLoop::current()->Run();

  CheckResponse(consumer, "HTTP/1.1 200", kResponseBody);
}

// crbug.com/536845
#if defined(THREAD_SANITIZER)
#define MAYBE_UberTest DISABLED_UberTest
#else
#define MAYBE_UberTest UberTest
#endif
TEST_F(QuicEndToEndTest, MAYBE_UberTest) {
  // FLAGS_fake_packet_loss_percentage = 30;

  const char kResponseBody[] = "some really big response body";
  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  std::vector<TestTransactionConsumer*> consumers;
  size_t num_requests = 100;
  for (size_t i = 0; i < num_requests; ++i) {
    TestTransactionConsumer* consumer = new TestTransactionConsumer(
        DEFAULT_PRIORITY, transaction_factory_.get());
    consumers.push_back(consumer);
    consumer->Start(&request_, BoundNetLog());
  }

  // Will terminate when the last consumer completes.
  base::MessageLoop::current()->Run();

  for (size_t i = 0; i < num_requests; ++i) {
    CheckResponse(*consumers[i], "HTTP/1.1 200", kResponseBody);
  }
  STLDeleteElements(&consumers);
}

}  // namespace test
}  // namespace net
