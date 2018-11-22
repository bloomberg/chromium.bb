// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/posix/eintr_wrapper.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_bridge.h"
#include "chrome/services/diagnosticsd/public/mojom/diagnosticsd.mojom.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_diagnosticsd_client.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class MockMojoDiagnosticsdService
    : public diagnosticsd::mojom::DiagnosticsdService {};

// Fake implementation of the DiagnosticsdServiceFactory Mojo interface that
// holds up method calls and allows to complete them afterwards.
class FakeMojoDiagnosticsdServiceFactory final
    : public diagnosticsd::mojom::DiagnosticsdServiceFactory {
 public:
  // DiagnosticsdServiceFactory overrides:

  void GetService(diagnosticsd::mojom::DiagnosticsdServiceRequest service,
                  diagnosticsd::mojom::DiagnosticsdClientPtr client,
                  GetServiceCallback callback) override {
    EXPECT_FALSE(pending_get_service_call_);
    pending_get_service_call_ = PendingGetServiceCall{
        std::move(service), std::move(client), std::move(callback)};
  }

  // Completes the Mojo binding of this instance to the given Mojo interface
  // request.
  void Bind(diagnosticsd::mojom::DiagnosticsdServiceFactoryRequest request) {
    // Close the Mojo binding in case it was previously completed, to allow
    // calling this method multiple times.
    self_binding_.Close();

    self_binding_.Bind(std::move(request));

    self_binding_.set_connection_error_handler(
        base::BindOnce(&FakeMojoDiagnosticsdServiceFactory::OnBindingError,
                       base::Unretained(this)));
  }

  // Closes the Mojo binding of this instance.
  void CloseBinding() {
    self_binding_.Close();

    // Drop the current pending GetService call, if there was any, to allow our
    // instance to be used for new GetService calls after this instance gets
    // bound again.
    pending_get_service_call_.reset();
  }

  // Whether there's a pending GetService call.
  bool is_get_service_call_in_flight() const {
    return pending_get_service_call_.has_value();
  }

  // Respond to the current pending GetService call.
  diagnosticsd::mojom::DiagnosticsdClientPtr RespondToGetServiceCall(
      mojo::Binding<diagnosticsd::mojom::DiagnosticsdService>*
          mojo_diagnosticsd_service_binding) {
    DCHECK(pending_get_service_call_);
    PendingGetServiceCall pending_call = std::move(*pending_get_service_call_);
    pending_get_service_call_.reset();
    mojo_diagnosticsd_service_binding->Bind(std::move(pending_call.service));
    std::move(pending_call.callback).Run();
    return std::move(pending_call.client);
  }

 private:
  struct PendingGetServiceCall {
    diagnosticsd::mojom::DiagnosticsdServiceRequest service;
    diagnosticsd::mojom::DiagnosticsdClientPtr client;
    GetServiceCallback callback;
  };

  void OnBindingError() {
    // Drop the current pending GetService call, if there was any, to allow our
    // instance to be used for new GetService calls after this instance gets
    // bound again.
    pending_get_service_call_.reset();
  }

  mojo::Binding<diagnosticsd::mojom::DiagnosticsdServiceFactory> self_binding_{
      this};

  base::Optional<PendingGetServiceCall> pending_get_service_call_;
};

// Fake implementation of the DiagnosticsdBridge delegate that simulates Mojo
// operations that are impossible in the unit test.
class FakeDiagnosticsdBridgeDelegate final
    : public DiagnosticsdBridge::Delegate {
 public:
  explicit FakeDiagnosticsdBridgeDelegate(
      FakeMojoDiagnosticsdServiceFactory* mojo_diagnosticsd_service_factory)
      : mojo_diagnosticsd_service_factory_(mojo_diagnosticsd_service_factory) {}

  void CreateDiagnosticsdServiceFactoryMojoInvitation(
      diagnosticsd::mojom::DiagnosticsdServiceFactoryPtr*
          diagnosticsd_service_factory_mojo_ptr,
      base::ScopedFD* remote_endpoint_fd) override {
    // Bind the Mojo pointer passed to the bridge with the
    // FakeMojoDiagnosticsdServiceFactory implementation.
    mojo_diagnosticsd_service_factory_->Bind(
        mojo::MakeRequest(diagnosticsd_service_factory_mojo_ptr));

    // Return a fake file descriptor - its value is not used in the unit test
    // environment for anything except comparing with zero.
    remote_endpoint_fd->reset(HANDLE_EINTR(dup(STDIN_FILENO)));
    DCHECK(remote_endpoint_fd->is_valid());
  }

 private:
  FakeMojoDiagnosticsdServiceFactory* const mojo_diagnosticsd_service_factory_;
};

// Tests for the DiagnosticsdBridge class.
class DiagnosticsdBridgeTest : public testing::Test {
 protected:
  DiagnosticsdBridgeTest() {
    DBusThreadManager::Initialize();
    CHECK(DBusThreadManager::Get()->IsUsingFakes());

    diagnosticsd_bridge_ = std::make_unique<DiagnosticsdBridge>(
        std::make_unique<FakeDiagnosticsdBridgeDelegate>(
            &mojo_diagnosticsd_service_factory_));
  }

  ~DiagnosticsdBridgeTest() override {
    diagnosticsd_bridge_.reset();
    DBusThreadManager::Shutdown();
  }

  base::TestMockTimeTaskRunner* task_runner() {
    return test_mock_time_task_runner_.get();
  }

  DiagnosticsdBridge* diagnosticsd_bridge() {
    return diagnosticsd_bridge_.get();
  }

  FakeDiagnosticsdClient* diagnosticsd_dbus_client() {
    DiagnosticsdClient* const diagnosticsd_client =
        DBusThreadManager::Get()->GetDiagnosticsdClient();
    DCHECK(diagnosticsd_client);
    return static_cast<FakeDiagnosticsdClient*>(diagnosticsd_client);
  }

  // Whether there's a pending GetService Mojo call held up by the fake.
  bool is_mojo_factory_get_service_call_in_flight() const {
    return mojo_diagnosticsd_service_factory_.is_get_service_call_in_flight();
  }

  // Reply to the pending GetService Mojo call held up by the fake.
  void RespondToMojoFactoryGetServiceCall() {
    // Close the binding, if it was completed, to allow calling this method
    // multiple times.
    mojo_diagnosticsd_service_binding_.Close();

    mojo_diagnosticsd_client_ =
        mojo_diagnosticsd_service_factory_.RespondToGetServiceCall(
            &mojo_diagnosticsd_service_binding_);
  }

  // Simulates Mojo connection error.
  void AbortMojoConnection() {
    mojo_diagnosticsd_service_factory_.CloseBinding();
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> test_mock_time_task_runner_ =
      new base::TestMockTimeTaskRunner(
          base::TestMockTimeTaskRunner::Type::kBoundToThread);

  FakeMojoDiagnosticsdServiceFactory mojo_diagnosticsd_service_factory_;

  MockMojoDiagnosticsdService mojo_diagnosticsd_service_;
  mojo::Binding<diagnosticsd::mojom::DiagnosticsdService>
      mojo_diagnosticsd_service_binding_{&mojo_diagnosticsd_service_};

  std::unique_ptr<DiagnosticsdBridge> diagnosticsd_bridge_;

  diagnosticsd::mojom::DiagnosticsdClientPtr mojo_diagnosticsd_client_;
};

}  // namespace

// Test successful Mojo bootstrapping scenario.
TEST_F(DiagnosticsdBridgeTest, SuccessfulBootstrap) {
  // Initially the bridge is blocked on the WaitForServiceToBeAvailable call.
  EXPECT_EQ(1, diagnosticsd_dbus_client()
                   ->wait_for_service_to_be_available_in_flight_call_count());
  EXPECT_FALSE(diagnosticsd_dbus_client()
                   ->bootstrap_mojo_connection_in_flight_call_count());

  // Resolve the pending WaitForServiceToBeAvailable call. Verify the bridge
  // makes the BootstrapMojoConnection D-Bus call.
  diagnosticsd_dbus_client()->SetWaitForServiceToBeAvailableResult(true);
  EXPECT_EQ(1, diagnosticsd_dbus_client()
                   ->bootstrap_mojo_connection_in_flight_call_count());

  // Resolve the pending BootstrapMojoConnection D-Bus call (but then revert the
  // fake in order to hold up its subsequent calls). Verify the bridge makes the
  // GetService Mojo call on the DiagnosticsdServiceFactory interface.
  diagnosticsd_dbus_client()->SetBootstrapMojoConnectionResult(true);
  diagnosticsd_dbus_client()->SetBootstrapMojoConnectionResult(base::nullopt);
  task_runner()->RunUntilIdle();
  ASSERT_TRUE(is_mojo_factory_get_service_call_in_flight());

  // Resolve the pending GetService Mojo call. Verify the bridge exposes the
  // obtained DiagnosticsdService Mojo interface pointer.
  RespondToMojoFactoryGetServiceCall();
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(diagnosticsd_bridge()->diagnosticsd_service_mojo_proxy());

  // Verify that no extra D-Bus or Mojo calls are made.
  EXPECT_FALSE(diagnosticsd_dbus_client()
                   ->bootstrap_mojo_connection_in_flight_call_count());
  EXPECT_FALSE(is_mojo_factory_get_service_call_in_flight());
}

// Test the case when the D-Bus service is permanently unavailable - the bridge
// should be just blocked on a single WaitForServiceToBeAvailable call.
TEST_F(DiagnosticsdBridgeTest, DBusServiceNotBringingUpError) {
  EXPECT_EQ(1, diagnosticsd_dbus_client()
                   ->wait_for_service_to_be_available_in_flight_call_count());

  // Verify that no extra WaitForServiceToBeAvailable calls are made.
  task_runner()->FastForwardBy(
      DiagnosticsdBridge::connection_attempt_interval_for_testing());
  EXPECT_EQ(1, diagnosticsd_dbus_client()
                   ->wait_for_service_to_be_available_in_flight_call_count());
}

// Test the case when the BootstrapMojoConnection D-Bus method fails
// permanently - the bridge should give up making attempts after a few retries.
TEST_F(DiagnosticsdBridgeTest, DBusBootstrapMojoConnectionError) {
  diagnosticsd_dbus_client()->SetWaitForServiceToBeAvailableResult(true);

  for (int attempt_number = 0;
       attempt_number <
       DiagnosticsdBridge::max_connection_attempt_count_for_testing();
       ++attempt_number) {
    // Fail the pending BootstrapMojoConnection call, but then revert the fake
    // in order to hold up its subsequent calls.
    EXPECT_EQ(1, diagnosticsd_dbus_client()
                     ->bootstrap_mojo_connection_in_flight_call_count());
    diagnosticsd_dbus_client()->SetBootstrapMojoConnectionResult(false);
    diagnosticsd_dbus_client()->SetBootstrapMojoConnectionResult(base::nullopt);
    task_runner()->RunUntilIdle();

    // Verify that no new BootstrapMojoConnection call is made immediately after
    // the previous one failed.
    EXPECT_FALSE(diagnosticsd_dbus_client()
                     ->bootstrap_mojo_connection_in_flight_call_count());

    // Fast forward the clock till the next attempt should occur.
    task_runner()->FastForwardBy(
        DiagnosticsdBridge::connection_attempt_interval_for_testing());
  }

  // No new BootstrapMojoConnection calls are made after the retry limit
  // exceeded.
  EXPECT_FALSE(diagnosticsd_dbus_client()
                   ->bootstrap_mojo_connection_in_flight_call_count());
}

// Test the case when the Mojo connection gets aborted before the first Mojo
// call (GetService on the DiagnosticsdServiceFactory interface) completes - the
// bridge should give up making attempts after a few retries.
TEST_F(DiagnosticsdBridgeTest, ImmediateMojoDisconnectionError) {
  diagnosticsd_dbus_client()->SetWaitForServiceToBeAvailableResult(true);
  diagnosticsd_dbus_client()->SetBootstrapMojoConnectionResult(true);
  task_runner()->RunUntilIdle();

  for (int attempt_number = 0;
       attempt_number <
       DiagnosticsdBridge::max_connection_attempt_count_for_testing();
       ++attempt_number) {
    // Verify that the bridge made the GetService Mojo call (on the
    // DiagnosticsdServiceFactory interface). Abort the Mojo binding without
    // responding to the call. Verify that no new call happens immediately.
    EXPECT_TRUE(is_mojo_factory_get_service_call_in_flight());
    AbortMojoConnection();
    task_runner()->RunUntilIdle();
    EXPECT_FALSE(is_mojo_factory_get_service_call_in_flight());

    // Fast forward the clock till the next attempt should occur.
    task_runner()->FastForwardBy(
        DiagnosticsdBridge::connection_attempt_interval_for_testing());
  }

  // No new connection attempts are made after the retry limit exceeded.
  EXPECT_FALSE(is_mojo_factory_get_service_call_in_flight());
}

// Test that the Mojo connection gets bootstrapped again after the previous one
// got aborted.
TEST_F(DiagnosticsdBridgeTest, Reestablishing) {
  // Let the bootstrapping succeed on the first attempt.
  diagnosticsd_dbus_client()->SetWaitForServiceToBeAvailableResult(true);
  diagnosticsd_dbus_client()->SetBootstrapMojoConnectionResult(true);
  task_runner()->RunUntilIdle();
  ASSERT_TRUE(is_mojo_factory_get_service_call_in_flight());
  RespondToMojoFactoryGetServiceCall();
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(diagnosticsd_bridge()->diagnosticsd_service_mojo_proxy());

  // Abort the Mojo binding. Verify that no new connection attempt happens
  // immediately.
  AbortMojoConnection();
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(diagnosticsd_bridge()->diagnosticsd_service_mojo_proxy());
  EXPECT_FALSE(is_mojo_factory_get_service_call_in_flight());

  // Fast forward the clock till the next connection attempt.
  task_runner()->FastForwardBy(
      DiagnosticsdBridge::connection_attempt_interval_for_testing());

  // Let the bootstrapping succeed again.
  ASSERT_TRUE(is_mojo_factory_get_service_call_in_flight());
  RespondToMojoFactoryGetServiceCall();
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(diagnosticsd_bridge()->diagnosticsd_service_mojo_proxy());
}

// Test that the bridge resets its retry counter after a successful
// bootstrapping takes place.
TEST_F(DiagnosticsdBridgeTest, RetryCounterReset) {
  diagnosticsd_dbus_client()->SetWaitForServiceToBeAvailableResult(true);

  // Fail the first few connection attempts, leaving only one attempt left.
  diagnosticsd_dbus_client()->SetBootstrapMojoConnectionResult(false);
  task_runner()->FastForwardBy(
      DiagnosticsdBridge::connection_attempt_interval_for_testing() *
      (DiagnosticsdBridge::max_connection_attempt_count_for_testing() - 2));

  // Let the bootstrapping succeed on the new attempt (the last allowed one in
  // this serie).
  diagnosticsd_dbus_client()->SetBootstrapMojoConnectionResult(true);
  task_runner()->FastForwardBy(
      DiagnosticsdBridge::connection_attempt_interval_for_testing());
  ASSERT_TRUE(is_mojo_factory_get_service_call_in_flight());
  RespondToMojoFactoryGetServiceCall();
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(diagnosticsd_bridge()->diagnosticsd_service_mojo_proxy());

  // Abort the Mojo binding.
  AbortMojoConnection();
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(diagnosticsd_bridge()->diagnosticsd_service_mojo_proxy());

  // Fail again a few attempts as before.
  diagnosticsd_dbus_client()->SetBootstrapMojoConnectionResult(false);
  task_runner()->FastForwardBy(
      DiagnosticsdBridge::connection_attempt_interval_for_testing() *
      (DiagnosticsdBridge::max_connection_attempt_count_for_testing() - 1));

  // Let the bootstrapping succeed again as before. Note that this verifies that
  // the retry attempts made before the previous successful bootstrap were
  // ignored.
  diagnosticsd_dbus_client()->SetBootstrapMojoConnectionResult(true);
  task_runner()->FastForwardBy(
      DiagnosticsdBridge::connection_attempt_interval_for_testing());
  ASSERT_TRUE(is_mojo_factory_get_service_call_in_flight());
  RespondToMojoFactoryGetServiceCall();
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(diagnosticsd_bridge()->diagnosticsd_service_mojo_proxy());
}

}  // namespace chromeos
