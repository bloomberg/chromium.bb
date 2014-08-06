// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_test_base.h"

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/services/test_service/test_request_tracker.mojom.h"
#include "mojo/services/test_service/test_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using mojo::test::ServiceReport;
using mojo::test::ServiceReportPtr;
using mojo::test::TestService;
using mojo::test::TestTimeService;
using mojo::test::TestServicePtr;
using mojo::test::TestTimeServicePtr;
using mojo::test::TestTrackedRequestService;
using mojo::test::TestTrackedRequestServicePtr;

namespace mojo {
namespace shell {
namespace test {
namespace {

void GetReportCallback(base::MessageLoop* loop,
                       std::vector<ServiceReport>* reports_out,
                       mojo::Array<ServiceReportPtr> report) {
  for (size_t i = 0; i < report.size(); i++)
    reports_out->push_back(*report[i]);
  loop->QuitWhenIdle();
}

class ShellTestBaseTest : public ShellTestBase {
 public:
  // Convenience helpers for use as callbacks in tests.
  template <typename T>
  base::Callback<void()> SetAndQuit(T* val, T result) {
      return base::Bind(&ShellTestBaseTest::SetAndQuitImpl<T>,
          base::Unretained(this), val, result);
  }
  template <typename T>
  base::Callback<void(T result)> SetAndQuit(T* val) {
      return base::Bind(&ShellTestBaseTest::SetAndQuitImpl<T>,
          base::Unretained(this), val);
  }
  static GURL test_app_url() {
    return GURL("mojo:mojo_test_app");
  }

  void GetReport(std::vector<ServiceReport>* report) {
    request_tracking_.Bind(
        ConnectToService(GURL("mojo:mojo_test_request_tracker_app"),
        TestTrackedRequestService::Name_).Pass());
    request_tracking_->GetReport(base::Bind(&GetReportCallback,
        base::Unretained(message_loop()),
        base::Unretained(report)));
    message_loop()->Run();
  }

 private:
  template<typename T>
  void SetAndQuitImpl(T* val, T result) {
    *val = result;
    message_loop()->QuitWhenIdle();
  }
  TestTrackedRequestServicePtr request_tracking_;
};

class QuitMessageLoopErrorHandler : public ErrorHandler {
 public:
  QuitMessageLoopErrorHandler() {}
  virtual ~QuitMessageLoopErrorHandler() {}

  // |ErrorHandler| implementation:
  virtual void OnConnectionError() OVERRIDE {
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuitMessageLoopErrorHandler);
};

// Tests that we can connect to a single service within a single app.
TEST_F(ShellTestBaseTest, ConnectBasic) {
  InterfacePtr<TestService> service;
  service.Bind(ConnectToService(test_app_url(), TestService::Name_).Pass());

  bool was_run = false;
  service->Ping(SetAndQuit<bool>(&was_run, true));
  message_loop()->Run();
  EXPECT_TRUE(was_run);
  EXPECT_FALSE(service.encountered_error());

  service.reset();

  // This will run until the test app has actually quit (which it will,
  // since we killed the only connection to it).
  message_loop()->Run();
}

// Tests that trying to connect to a service fails properly if the service
// doesn't exist. Implicit in this test is verification that the shell
// terminates if no services are running.
TEST_F(ShellTestBaseTest, ConnectInvalidService) {
  InterfacePtr<TestService> test_service;
  test_service.Bind(ConnectToService(GURL("mojo:non_existent_service"),
                                     TestService::Name_).Pass());

  bool was_run = false;
  test_service->Ping(SetAndQuit<bool>(&was_run, true));

  // This will quit because there's nothing running.
  message_loop()->Run();
  EXPECT_FALSE(was_run);

  // It may have quit before an error was processed.
  if (!test_service.encountered_error()) {
    QuitMessageLoopErrorHandler quitter;
    test_service.set_error_handler(&quitter);
    message_loop()->Run();
    EXPECT_TRUE(test_service.encountered_error());
  }

  test_service.reset();
}

// Tests that we can connect to a single service within a single app using
// a network based loader instead of local files.
// TODO(tim): Disabled because network service leaks NSS at exit, meaning
// subsequent tests can't init properly.
TEST_F(ShellTestBaseTest, DISABLED_ConnectBasicNetwork) {
  InterfacePtr<TestService> service;
  service.Bind(ConnectToServiceViaNetwork(
      test_app_url(), TestService::Name_).Pass());

  bool was_run = false;
  service->Ping(SetAndQuit<bool>(&was_run, true));
  message_loop()->Run();
  EXPECT_TRUE(was_run);
  EXPECT_FALSE(service.encountered_error());

  // Note that use of the network service is implicit in this test.
  // Since TestService is not the only service in use, the shell won't auto
  // magically exit when TestService is destroyed (unlike ConnectBasic).
  // Tearing down the shell context will kill connections. The shell loop will
  // exit as soon as no more apps are connected.
  // TODO(tim): crbug.com/392685.  Calling this explicitly shouldn't be
  // necessary once the shell terminates if the primordial app exits, which
  // we could enforce here by resetting |service|.
  shell_context()->service_manager()->TerminateShellConnections();
  message_loop()->Run();  // Waits for all connections to die.
}

// Tests that trying to connect to a service over network fails preoprly
// if the service doesn't exist.
// TODO(tim): Disabled because network service leaks NSS at exit, meaning
// subsequent tests can't init properly.
TEST_F(ShellTestBaseTest, DISABLED_ConnectInvalidServiceNetwork) {
  InterfacePtr<TestService> test_service;
  test_service.Bind(ConnectToServiceViaNetwork(
      GURL("mojo:non_existent_service"), TestService::Name_).Pass());
  QuitMessageLoopErrorHandler quitter;
  test_service.set_error_handler(&quitter);
  bool was_run = false;
  test_service->Ping(SetAndQuit<bool>(&was_run, true));
  message_loop()->Run();
  EXPECT_TRUE(test_service.encountered_error());

  // TODO(tim): crbug.com/392685.  Calling this explicitly shouldn't be
  // necessary once the shell terminates if the primordial app exits, which
  // we could enforce here by resetting |service|.
  shell_context()->service_manager()->TerminateShellConnections();
  message_loop()->Run();  // Waits for all connections to die.
}

// Similar to ConnectBasic, but causes the app to instantiate multiple
// service implementation objects and verifies the shell can reach both.
TEST_F(ShellTestBaseTest, ConnectMultipleInstancesPerApp) {
  {
    TestServicePtr service1, service2;
    service1.Bind(ConnectToService(test_app_url(), TestService::Name_).Pass());
    service2.Bind(ConnectToService(test_app_url(), TestService::Name_).Pass());

    bool was_run1 = false;
    bool was_run2 = false;
    service1->Ping(SetAndQuit<bool>(&was_run1, true));
    message_loop()->Run();
    service2->Ping(SetAndQuit<bool>(&was_run2, true));
    message_loop()->Run();
    EXPECT_TRUE(was_run1);
    EXPECT_TRUE(was_run2);
    EXPECT_FALSE(service1.encountered_error());
    EXPECT_FALSE(service2.encountered_error());
  }
  message_loop()->Run();
}

// Tests that service A and service B, both in App 1, can talk to each other
// and parameters are passed around properly.
TEST_F(ShellTestBaseTest, ConnectDifferentServicesInSingleApp) {
  // Have a TestService GetPartyTime on a TestTimeService in the same app.
  int64 time_message;
  TestServicePtr service;
  service.Bind(ConnectToService(test_app_url(), TestService::Name_).Pass());
  service->ConnectToAppAndGetTime(test_app_url().spec(),
                                  SetAndQuit<int64>(&time_message));
  message_loop()->Run();

  // Verify by hitting the TimeService directly.
  TestTimeServicePtr time_service;
  time_service.Bind(
      ConnectToService(test_app_url(), TestTimeService::Name_).Pass());
  int64 party_time;
  time_service->GetPartyTime(SetAndQuit<int64>(&party_time));
  message_loop()->Run();

  EXPECT_EQ(time_message, party_time);
}

// Tests that a service A in App 1 can talk to service B in App 2 and
// parameters are passed around properly.
TEST_F(ShellTestBaseTest, ConnectDifferentServicesInDifferentApps) {
  int64 time_message;
  TestServicePtr service;
  service.Bind(ConnectToService(test_app_url(), TestService::Name_).Pass());
  service->ConnectToAppAndGetTime("mojo:mojo_test_request_tracker_app",
                                  SetAndQuit<int64>(&time_message));
  message_loop()->Run();

  // Verify by hitting the TimeService in the request tracker app directly.
  TestTimeServicePtr time_service;
  time_service.Bind(ConnectToService(GURL("mojo:mojo_test_request_tracker_app"),
                    TestTimeService::Name_).Pass());
  int64 party_time;
  time_service->GetPartyTime(SetAndQuit<int64>(&party_time));
  message_loop()->Run();

  EXPECT_EQ(time_message, party_time);
}

// Tests that service A in App 1 can be a client of service B in App 2.
TEST_F(ShellTestBaseTest, ConnectServiceAsClientOfSeparateApp) {
  TestServicePtr service;
  service.Bind(ConnectToService(test_app_url(), TestService::Name_).Pass());
  service->StartTrackingRequests(message_loop()->QuitWhenIdleClosure());
  service->Ping(mojo::Callback<void()>());
  message_loop()->Run();

  for (int i = 0; i < 8; i++)
    service->Ping(mojo::Callback<void()>());
  service->Ping(message_loop()->QuitWhenIdleClosure());
  message_loop()->Run();

  // If everything worked properly, the tracking service should report
  // 10 pings to TestService.
  std::vector<ServiceReport> reports;
  GetReport(&reports);
  ASSERT_EQ(1U, reports.size());
  EXPECT_EQ(TestService::Name_, reports[0].service_name);
  EXPECT_EQ(10U, reports[0].total_requests);
}

// Connect several services together and use the tracking service to verify
// communication.
TEST_F(ShellTestBaseTest, ConnectManyClientsAndServices) {
  TestServicePtr service;
  TestTimeServicePtr time_service;

  // Make a request to the TestService and have it contact TimeService in the
  // tracking app. Do all this with tracking enabled, meaning both services
  // are connected as clients of the TrackedRequestService.
  service.Bind(ConnectToService(test_app_url(), TestService::Name_).Pass());
  service->StartTrackingRequests(message_loop()->QuitWhenIdleClosure());
  message_loop()->Run();
  for (int i = 0; i < 5; i++)
    service->Ping(mojo::Callback<void()>());
  int64 time_result;
  service->ConnectToAppAndGetTime("mojo:mojo_test_request_tracker_app",
                                  SetAndQuit<int64>(&time_result));
  message_loop()->Run();

  // Also make a few requests to the TimeService in the test_app.
  time_service.Bind(
      ConnectToService(test_app_url(), TestTimeService::Name_).Pass());
  time_service->StartTrackingRequests(message_loop()->QuitWhenIdleClosure());
  time_service->GetPartyTime(mojo::Callback<void(uint64_t)>());
  message_loop()->Run();
  for (int i = 0; i < 18; i++)
    time_service->GetPartyTime(mojo::Callback<void(uint64_t)>());
  // Flush the tasks with one more to quit.
  int64 party_time = 0;
  time_service->GetPartyTime(SetAndQuit<int64>(&party_time));
  message_loop()->Run();

  std::vector<ServiceReport> reports;
  GetReport(&reports);
  ASSERT_EQ(3U, reports.size());
  EXPECT_EQ(TestService::Name_, reports[0].service_name);
  EXPECT_EQ(6U, reports[0].total_requests);
  EXPECT_EQ(TestTimeService::Name_, reports[1].service_name);
  EXPECT_EQ(1U, reports[1].total_requests);
  EXPECT_EQ(TestTimeService::Name_, reports[2].service_name);
  EXPECT_EQ(20U, reports[2].total_requests);
}

}  // namespace
}  // namespace test
}  // namespace shell
}  // namespace mojo
