// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/connection_tester.h"

#include "net/base/mock_host_resolver.h"
#include "net/url_request/url_request_unittest.h"
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
  ConnectionTesterTest() : message_loop_(MessageLoop::TYPE_IO) {
    scoped_refptr<net::RuleBasedHostResolverProc> catchall_resolver =
        new net::RuleBasedHostResolverProc(NULL);

    catchall_resolver->AddRule("*", "127.0.0.1");

    scoped_host_resolver_proc_.Init(catchall_resolver);
  }

 protected:
  net::ScopedDefaultHostResolverProc scoped_host_resolver_proc_;
  ConnectionTesterDelegate test_delegate_;
  MessageLoop message_loop_;
};

TEST_F(ConnectionTesterTest, RunAllTests) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateForkingServer(L"net/data/url_request_unittest/");

  ConnectionTester tester(&test_delegate_);

  // Start the test suite on URL "echoall".
  // TODO(eroman): Is this URL right?
  GURL url = server->TestServerPage("echoall");
  tester.RunAllTests(url);

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
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateForkingServer(L"net/data/url_request_unittest/");

  scoped_ptr<ConnectionTester> tester(new ConnectionTester(&test_delegate_));

  // Start the test suite on URL "echoall".
  // TODO(eroman): Is this URL right?
  GURL url = server->TestServerPage("echoall");
  tester->RunAllTests(url);

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

