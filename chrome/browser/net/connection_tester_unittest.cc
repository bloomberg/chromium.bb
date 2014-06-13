// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/connection_tester.h"

#include "base/prefs/testing_pref_service.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using content::BrowserThread;

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

  virtual void OnStartConnectionTestSuite() OVERRIDE {
    start_connection_test_suite_count_++;
  }

  virtual void OnStartConnectionTestExperiment(
      const ConnectionTester::Experiment& experiment) OVERRIDE {
    start_connection_test_experiment_count_++;
  }

  virtual void OnCompletedConnectionTestExperiment(
      const ConnectionTester::Experiment& experiment,
      int result) OVERRIDE {
    completed_connection_test_experiment_count_++;
  }

  virtual void OnCompletedConnectionTestSuite() OVERRIDE {
    completed_connection_test_suite_count_++;
    base::MessageLoop::current()->Quit();
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
      : io_thread_(BrowserThread::IO, &message_loop_),
        test_server_(net::SpawnedTestServer::TYPE_HTTP,
                     net::SpawnedTestServer::kLocalhost,
                     // Nothing is read in this directory.
                     base::FilePath(FILE_PATH_LITERAL("chrome"))) {
  }

 protected:
  // Destroy last the MessageLoop last to give a chance for objects like
  // ObserverListThreadSave to shut down properly.  For example,
  // SSLClientAuthCache calls RemoveObserver when destroyed, but if the
  // MessageLoop is already destroyed, then the RemoveObserver will be a
  // no-op, and the ObserverList will contain invalid entries.
  base::MessageLoopForIO message_loop_;
  content::TestBrowserThread io_thread_;
  net::SpawnedTestServer test_server_;
  net::TestURLRequestContext proxy_script_fetcher_context_;
  ConnectionTesterDelegate test_delegate_;
};

TEST_F(ConnectionTesterTest, RunAllTests) {
  ASSERT_TRUE(test_server_.Start());

  ConnectionTester tester(&test_delegate_,
                          &proxy_script_fetcher_context_,
                          NULL);

  // Start the test suite on URL "echoall".
  // TODO(eroman): Is this URL right?
  tester.RunAllTests(test_server_.GetURL("echoall"));

  // Wait for all the tests to complete.
  base::MessageLoop::current()->Run();

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
      new ConnectionTester(&test_delegate_,
                           &proxy_script_fetcher_context_,
                           NULL));

  // Start the test suite on URL "echoall".
  // TODO(eroman): Is this URL right?
  tester->RunAllTests(test_server_.GetURL("echoall"));

  // Don't run the message loop at all.  Otherwise the experiment's request may
  // complete and post a task to run the next experiment before we quit the
  // message loop.

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
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
  base::MessageLoop::current()->Run();
}

}  // namespace
