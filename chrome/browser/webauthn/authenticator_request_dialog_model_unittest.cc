// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/webauthn/transport_list_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const base::flat_set<device::FidoTransportProtocol> kAllTransports = {
    device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
    device::FidoTransportProtocol::kNearFieldCommunication,
    device::FidoTransportProtocol::kBluetoothLowEnergy,
    device::FidoTransportProtocol::kInternal,
    device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
};

using TransportAvailabilityInfo =
    ::device::FidoRequestHandlerBase::TransportAvailabilityInfo;

class MockDialogModelObserver
    : public testing::StrictMock<AuthenticatorRequestDialogModel::Observer> {
 public:
  MockDialogModelObserver() = default;

  MOCK_METHOD0(OnModelDestroyed, void());
  MOCK_METHOD0(OnStepTransition, void());
  MOCK_METHOD0(OnCancelRequest, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDialogModelObserver);
};

}  // namespace

class AuthenticatorRequestDialogModelTest : public ::testing::Test {
 public:
  AuthenticatorRequestDialogModelTest() {}
  ~AuthenticatorRequestDialogModelTest() override {}

 protected:
  base::test::ScopedTaskEnvironment task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestDialogModelTest);
};

TEST_F(AuthenticatorRequestDialogModelTest, TransportAutoSelection) {
  using FidoTransportProtocol = ::device::FidoTransportProtocol;
  using Step = AuthenticatorRequestDialogModel::Step;
  using RequestType = ::device::FidoRequestHandlerBase::RequestType;

  const struct {
    RequestType request_type;
    base::flat_set<FidoTransportProtocol> available_transports;
    base::Optional<FidoTransportProtocol> last_used_transport;
    bool has_touch_id_credential;
    Step expected_first_step;
  } kTestCases[] = {
      // Only a single transport is available.
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kUsbHumanInterfaceDevice},
       base::nullopt,
       false,
       Step::kUsbInsertAndActivate},
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kNearFieldCommunication},
       base::nullopt,
       false,
       Step::kTransportSelection},
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kBluetoothLowEnergy},
       base::nullopt,
       false,
       Step::kBleActivate},
      {RequestType::kMakeCredential,
       {FidoTransportProtocol::kInternal},
       base::nullopt,
       false,
       Step::kTouchId},
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kInternal},
       base::nullopt,
       false,
       Step::kErrorInternalUnrecognized},
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kInternal},
       base::nullopt,
       true,
       Step::kTouchId},
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy},
       base::nullopt,
       false,
       Step::kCableActivate},

      // The last used transport is available.
      {RequestType::kGetAssertion, kAllTransports,
       FidoTransportProtocol::kUsbHumanInterfaceDevice, false,
       Step::kUsbInsertAndActivate},

      // The last used transport is not available.
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
        FidoTransportProtocol::kUsbHumanInterfaceDevice},
       FidoTransportProtocol::kNearFieldCommunication,
       false,
       Step::kTransportSelection},

      // The KeyChain contains an allowed Touch ID credential.
      {RequestType::kGetAssertion, kAllTransports,
       FidoTransportProtocol::kUsbHumanInterfaceDevice, true, Step::kTouchId},

      // The KeyChain does not contain an allowed Touch ID credential.
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kInternal},
       FidoTransportProtocol::kUsbHumanInterfaceDevice,
       false,
       Step::kErrorInternalUnrecognized},
      {RequestType::kGetAssertion, kAllTransports,
       FidoTransportProtocol::kUsbHumanInterfaceDevice, false,
       Step::kUsbInsertAndActivate},
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kUsbHumanInterfaceDevice,
        FidoTransportProtocol::kInternal},
       FidoTransportProtocol::kInternal,
       false,
       Step::kUsbInsertAndActivate},
      {RequestType::kGetAssertion, kAllTransports, base::nullopt, false,
       Step::kTransportSelection},

      // The KeyChain contains an allowed Touch ID credential, but Touch ID is
      // not enabled by the relying party.
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kUsbHumanInterfaceDevice},
       base::nullopt,
       true,
       Step::kUsbInsertAndActivate},
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kUsbHumanInterfaceDevice,
        FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy},
       base::nullopt,
       true,
       Step::kTransportSelection},

      // No transports available.
      {RequestType::kGetAssertion,
       {},
       FidoTransportProtocol::kNearFieldCommunication,
       false,
       Step::kErrorNoAvailableTransports},

      // Even when last transport used is available, we default to transport
      // selection modal for MakeCredential.
      {RequestType::kMakeCredential, kAllTransports,
       FidoTransportProtocol::kUsbHumanInterfaceDevice, false,
       Step::kTransportSelection},

      // When only one transport is available, we still want to skip transport
      // selection view for MakeCredential call.
      {RequestType::kMakeCredential,
       {FidoTransportProtocol::kUsbHumanInterfaceDevice},
       FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
       false,
       Step::kUsbInsertAndActivate},
      {RequestType::kMakeCredential,
       {FidoTransportProtocol::kUsbHumanInterfaceDevice},
       FidoTransportProtocol::kUsbHumanInterfaceDevice,
       false,
       Step::kUsbInsertAndActivate},
  };

  for (const auto& test_case : kTestCases) {
    TransportAvailabilityInfo transports_info;
    transports_info.request_type = test_case.request_type;
    transports_info.available_transports = test_case.available_transports;
    transports_info.has_recognized_mac_touch_id_credential =
        test_case.has_touch_id_credential;

    AuthenticatorRequestDialogModel model;
    model.StartFlow(std::move(transports_info), test_case.last_used_transport);
    EXPECT_EQ(test_case.expected_first_step, model.current_step());

    if (model.current_step() == Step::kTransportSelection)
      continue;

    model.Back();
    EXPECT_EQ(Step::kTransportSelection, model.current_step());
  }
}

TEST_F(AuthenticatorRequestDialogModelTest, TransportList) {
  TransportAvailabilityInfo transports_info;
  transports_info.available_transports = kAllTransports;

  AuthenticatorRequestDialogModel model;
  model.StartFlow(std::move(transports_info), base::nullopt);
  EXPECT_THAT(model.transport_list_model()->transports(),
              ::testing::UnorderedElementsAre(
                  AuthenticatorTransport::kUsbHumanInterfaceDevice,
                  AuthenticatorTransport::kNearFieldCommunication,
                  AuthenticatorTransport::kBluetoothLowEnergy,
                  AuthenticatorTransport::kInternal,
                  AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy));
}

TEST_F(AuthenticatorRequestDialogModelTest, NoAvailableTransports) {
  using Step = AuthenticatorRequestDialogModel::Step;

  MockDialogModelObserver mock_observer;
  AuthenticatorRequestDialogModel model;
  model.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnStepTransition());
  model.StartFlow(TransportAvailabilityInfo(),
                  AuthenticatorTransport::kInternal);
  EXPECT_EQ(Step::kErrorNoAvailableTransports, model.current_step());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnCancelRequest());
  model.Cancel();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnStepTransition());
  model.OnRequestComplete();
  EXPECT_EQ(Step::kClosed, model.current_step());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnModelDestroyed());
}

TEST_F(AuthenticatorRequestDialogModelTest, PostMortems) {
  using Step = AuthenticatorRequestDialogModel::Step;

  const struct {
    void (AuthenticatorRequestDialogModel::*event)();
    Step expected_post_mortem_sheet;
  } kTestCases[] = {
      {&AuthenticatorRequestDialogModel::OnRequestTimeout,
       Step::kPostMortemTimedOut},
      {&AuthenticatorRequestDialogModel::OnActivatedKeyNotRegistered,
       Step::kPostMortemKeyNotRegistered},
      {&AuthenticatorRequestDialogModel::OnActivatedKeyAlreadyRegistered,
       Step::kPostMortemKeyAlreadyRegistered},
  };

  for (const auto& test_case : kTestCases) {
    MockDialogModelObserver mock_observer;
    AuthenticatorRequestDialogModel model;
    model.AddObserver(&mock_observer);

    TransportAvailabilityInfo transports_info;
    transports_info.available_transports = kAllTransports;

    EXPECT_CALL(mock_observer, OnStepTransition());
    model.StartFlow(std::move(transports_info), base::nullopt);
    EXPECT_EQ(Step::kTransportSelection, model.current_step());
    testing::Mock::VerifyAndClearExpectations(&mock_observer);

    EXPECT_CALL(mock_observer, OnStepTransition());
    (model.*test_case.event)();
    model.OnRequestComplete();
    EXPECT_EQ(test_case.expected_post_mortem_sheet, model.current_step());
    testing::Mock::VerifyAndClearExpectations(&mock_observer);

    EXPECT_CALL(mock_observer, OnStepTransition());
    model.Cancel();
    EXPECT_EQ(Step::kClosed, model.current_step());
    testing::Mock::VerifyAndClearExpectations(&mock_observer);

    EXPECT_CALL(mock_observer, OnModelDestroyed());
  }
}

TEST_F(AuthenticatorRequestDialogModelTest,
       RequestCallbackOnlyCalledOncePerAuthenticator) {
  ::device::FidoRequestHandlerBase::TransportAvailabilityInfo transports_info;
  transports_info.request_type =
      device::FidoRequestHandlerBase::RequestType::kMakeCredential;
  transports_info.available_transports = {
      AuthenticatorTransport::kInternal,
      AuthenticatorTransport::kUsbHumanInterfaceDevice};

  int num_called = 0;
  AuthenticatorRequestDialogModel model;
  model.SetRequestCallback(base::BindRepeating(
      [](int* i, const std::string& authenticator_id) { ++(*i); },
      &num_called));
  model.saved_authenticators().emplace_back(
      AuthenticatorRequestDialogModel::AuthenticatorReference(
          "authenticator", AuthenticatorTransport::kInternal));

  model.StartFlow(std::move(transports_info), base::nullopt);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kTransportSelection,
            model.current_step());
  EXPECT_EQ(0, num_called);

  // Simulate switching back and forth between transports. The request callback
  // should only be invoked once (USB is not dispatched through the UI).
  model.StartGuidedFlowForTransport(AuthenticatorTransport::kInternal);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kTouchId,
            model.current_step());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
  model.StartGuidedFlowForTransport(
      AuthenticatorTransport::kUsbHumanInterfaceDevice);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kUsbInsertAndActivate,
            model.current_step());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
  model.StartGuidedFlowForTransport(AuthenticatorTransport::kInternal);
  EXPECT_EQ(AuthenticatorRequestDialogModel::Step::kTouchId,
            model.current_step());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, num_called);
}
