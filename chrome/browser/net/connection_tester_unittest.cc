// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/connection_tester.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/test/testing_pref_service.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/socket/client_socket_factory.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// This is a testing delegate which simply counts how many times each of
// the delegate's methods were invoked.
class ConnectionTesterDelegate : public ConnectionTester::Delegate {
 public:
  ConnectionTesterDelegate()
     : start_connection_test_suite_count_(0),
       start_connection_test_experiment_count_(0),
       completed_connection_test_experiment_count_(0),
       completed_connection_test_suite_count_(0) {
  }

  virtual void OnStartConnectionTestSuite() {
    start_connection_test_suite_count_++;
  }

  virtual void OnStartConnectionTestExperiment(
      const ConnectionTester::Experiment& experiment) {
    start_connection_test_experiment_count_++;
  }

  virtual void OnCompletedConnectionTestExperiment(
      const ConnectionTester::Experiment& experiment,
      int result) {
    completed_connection_test_experiment_count_++;
  }

  virtual void OnCompletedConnectionTestSuite() {
    completed_connection_test_suite_count_++;
    MessageLoop::current()->Quit();
  }

  int start_connection_test_suite_count() const {
    return start_connection_test_suite_count_;
  }

  int start_connection_test_experiment_count() const {
    return start_connection_test_experiment_count_;
  }

  int completed_connection_test_experiment_count() const {
    return completed_connection_test_experiment_count_;
  }

  int completed_connection_test_suite_count() const {
    return completed_connection_test_suite_count_;
  }

 private:
  int start_connection_test_suite_count_;
  int start_connection_test_experiment_count_;
  int completed_connection_test_experiment_count_;
  int completed_connection_test_suite_count_;
};

// The test fixture is responsible for:
//   - Making sure each test has an IO loop running
//   - Catching any host resolve requests and mapping them to localhost
//     (so the test doesn't use any external network dependencies).
class ConnectionTesterTest : public PlatformTest {
 public:
  ConnectionTesterTest()
      : test_server_(net::TestServer::TYPE_HTTP,
            FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest"))),
        client_socket_factory_(net::ClientSocketFactory::GetDefaultFactory()),
        proxy_script_fetcher_context_(new net::URLRequestContext),
        message_loop_(MessageLoop::TYPE_IO),
        io_thread_(BrowserThread::IO, &message_loop_) {
    InitializeRequestContext();
  }

 protected:
  net::TestServer test_server_;
  ConnectionTesterDelegate test_delegate_;
  net::ClientSocketFactory* const client_socket_factory_;
  net::MockHostResolver host_resolver_;
  net::CertVerifier cert_verifier_;
  net::DnsRRResolver dnsrr_resolver_;
  scoped_refptr<net::ProxyService> proxy_service_;
  scoped_refptr<net::SSLConfigService> ssl_config_service_;
  scoped_ptr<net::HttpTransactionFactory> http_transaction_factory_;
  net::HttpAuthHandlerRegistryFactory http_auth_handler_factory_;
  scoped_refptr<net::URLRequestContext> proxy_script_fetcher_context_;

 private:
  void InitializeRequestContext() {
    proxy_script_fetcher_context_->set_host_resolver(&host_resolver_);
    proxy_script_fetcher_context_->set_cert_verifier(&cert_verifier_);
    proxy_script_fetcher_context_->set_dnsrr_resolver(&dnsrr_resolver_);
    proxy_script_fetcher_context_->set_http_auth_handler_factory(
        &http_auth_handler_factory_);
    proxy_service_ = net::ProxyService::CreateDirect();
    proxy_script_fetcher_context_->set_proxy_service(proxy_service_);
    ssl_config_service_ = net::SSLConfigService::CreateSystemSSLConfigService();
    scoped_refptr<net::HttpNetworkSession> network_session(
        new net::HttpNetworkSession(
            &host_resolver_,
            &cert_verifier_,
            &dnsrr_resolver_,
            NULL /* dns_cert_checker */,
            NULL /* ssl_host_info_factory */,
            proxy_service_.get(),
            client_socket_factory_,
            ssl_config_service_,
            new net::SpdySessionPool(ssl_config_service_),
            &http_auth_handler_factory_,
            NULL /* network_delegate */,
            NULL /* net_log */));
    http_transaction_factory_.reset(
        new net::HttpNetworkLayer(network_session));
    proxy_script_fetcher_context_->set_http_transaction_factory(
        http_transaction_factory_.get());
    // In-memory cookie store.
    proxy_script_fetcher_context_->set_cookie_store(
        new net::CookieMonster(NULL, NULL));
  }

  MessageLoop message_loop_;
  BrowserThread io_thread_;
};

TEST_F(ConnectionTesterTest, RunAllTests) {
  ASSERT_TRUE(test_server_.Start());

  ConnectionTester tester(&test_delegate_, proxy_script_fetcher_context_);

  // Start the test suite on URL "echoall".
  // TODO(eroman): Is this URL right?
  tester.RunAllTests(test_server_.GetURL("echoall"));

  // Wait for all the tests to complete.
  MessageLoop::current()->Run();

  const int kNumExperiments =
      ConnectionTester::PROXY_EXPERIMENT_COUNT *
      ConnectionTester::HOST_RESOLVER_EXPERIMENT_COUNT;

  EXPECT_EQ(1, test_delegate_.start_connection_test_suite_count());
  EXPECT_EQ(kNumExperiments,
            test_delegate_.start_connection_test_experiment_count());
  EXPECT_EQ(kNumExperiments,
            test_delegate_.completed_connection_test_experiment_count());
  EXPECT_EQ(1, test_delegate_.completed_connection_test_suite_count());
}

TEST_F(ConnectionTesterTest, DeleteWhileInProgress) {
  ASSERT_TRUE(test_server_.Start());

  scoped_ptr<ConnectionTester> tester(
      new ConnectionTester(&test_delegate_, proxy_script_fetcher_context_));

  // Start the test suite on URL "echoall".
  // TODO(eroman): Is this URL right?
  tester->RunAllTests(test_server_.GetURL("echoall"));

  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(1, test_delegate_.start_connection_test_suite_count());
  EXPECT_EQ(1, test_delegate_.start_connection_test_experiment_count());
  EXPECT_EQ(0, test_delegate_.completed_connection_test_experiment_count());
  EXPECT_EQ(0, test_delegate_.completed_connection_test_suite_count());

  // Delete the ConnectionTester while it is in progress.
  tester.reset();

  // Drain the tasks on the message loop.
  //
  // Note that we cannot simply stop the message loop, since that will delete
  // any pending tasks instead of running them. This causes a problem with
  // net::ClientSocketPoolBaseHelper, since the "Group" holds a pointer
  // |backup_task| that it will try to deref during the destructor, but
  // depending on the order that pending tasks were deleted in, it might
  // already be invalid! See http://crbug.com/43291.
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  MessageLoop::current()->Run();
}

}  // namespace
