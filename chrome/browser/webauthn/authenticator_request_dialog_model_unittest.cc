// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <utility>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
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
}

class AuthenticatorRequestDialogModelTest : public ::testing::Test {
 public:
  AuthenticatorRequestDialogModelTest() {}
  ~AuthenticatorRequestDialogModelTest() override {}

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
      {RequestType::kGetAssertion,
       {FidoTransportProtocol::kInternal},
       base::nullopt,
       false,
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
    ::device::FidoRequestHandlerBase::TransportAvailabilityInfo transports_info;
    transports_info.request_type = test_case.request_type;
    transports_info.available_transports = test_case.available_transports;
    transports_info.has_recognized_mac_touch_id_credential =
        test_case.has_touch_id_credential;

    AuthenticatorRequestDialogModel model;
    model.StartFlow(std::move(transports_info), test_case.last_used_transport);

    // Expect and advance through the welcome screen if this is the first time
    // the user goes through the flow (there is no |last_used_transport|.
    if (!test_case.last_used_transport) {
      ASSERT_EQ(Step::kWelcomeScreen, model.current_step());
      model.StartGuidedFlowForMostLikelyTransportOrShowTransportSelection();
    }
    EXPECT_EQ(test_case.expected_first_step, model.current_step());

    if (model.current_step() == Step::kTransportSelection)
      continue;

    model.Back();
    if (test_case.available_transports.size() >= 2u) {
      EXPECT_EQ(Step::kTransportSelection, model.current_step());
    } else {
      EXPECT_EQ(Step::kWelcomeScreen, model.current_step());
    }
  }
}

TEST_F(AuthenticatorRequestDialogModelTest, TransportList) {
  ::device::FidoRequestHandlerBase::TransportAvailabilityInfo transports_info_1;
  transports_info_1.available_transports = kAllTransports;

  AuthenticatorRequestDialogModel model;
  model.StartFlow(std::move(transports_info_1), base::nullopt);
  EXPECT_THAT(model.transport_list_model()->transports(),
              ::testing::UnorderedElementsAre(
                  AuthenticatorTransport::kUsb,
                  AuthenticatorTransport::kNearFieldCommunication,
                  AuthenticatorTransport::kBluetoothLowEnergy,
                  AuthenticatorTransport::kInternal,
                  AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy));
}
