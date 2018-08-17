// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/webauthn/transport_list_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  const base::flat_set<FidoTransportProtocol> kAllTransports = {
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      FidoTransportProtocol::kNearFieldCommunication,
      FidoTransportProtocol::kBluetoothLowEnergy,
      FidoTransportProtocol::kInternal,
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
  };

  const struct {
    base::flat_set<FidoTransportProtocol> available_transports;
    base::Optional<FidoTransportProtocol> last_used_transport;
    bool has_touch_id_credential;
    Step expected_first_step;
  } kTestCases[] = {
      // Only a single transport is available.
      {{FidoTransportProtocol::kUsbHumanInterfaceDevice},
       base::nullopt,
       false,
       Step::kUsbInsertAndActivate},
      {{FidoTransportProtocol::kNearFieldCommunication},
       base::nullopt,
       false,
       Step::kTransportSelection},
      {{FidoTransportProtocol::kBluetoothLowEnergy},
       base::nullopt,
       false,
       Step::kBleActivate},
      {{FidoTransportProtocol::kInternal},
       base::nullopt,
       false,
       Step::kTouchId},
      {{FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy},
       base::nullopt,
       false,
       Step::kCableActivate},

      // The last used transport is available.
      {kAllTransports, FidoTransportProtocol::kUsbHumanInterfaceDevice, false,
       Step::kUsbInsertAndActivate},

      // The last used transport is not available.
      {{FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
        FidoTransportProtocol::kUsbHumanInterfaceDevice},
       FidoTransportProtocol::kNearFieldCommunication,
       false,
       Step::kTransportSelection},

      // The KeyChain contains an allowed Touch ID credential.
      {kAllTransports, FidoTransportProtocol::kUsbHumanInterfaceDevice, true,
       Step::kTouchId},

      // The KeyChain contains an allowed Touch ID credential, but Touch ID is
      // not enabled by the relying party.
      {{FidoTransportProtocol::kUsbHumanInterfaceDevice},
       base::nullopt,
       true,
       Step::kUsbInsertAndActivate},
      {{FidoTransportProtocol::kUsbHumanInterfaceDevice,
        FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy},
       base::nullopt,
       true,
       Step::kTransportSelection},

      // No transports available.
      {{},
       FidoTransportProtocol::kNearFieldCommunication,
       false,
       // TODO: Update this when the error screen is implemented.
       Step::kTransportSelection},
  };

  for (const auto& test_case : kTestCases) {
    ::device::FidoRequestHandlerBase::TransportAvailabilityInfo transports_info;
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
  }
}

TEST_F(AuthenticatorRequestDialogModelTest, TransportList) {
  using FidoTransportProtocol = ::device::FidoTransportProtocol;

  ::device::FidoRequestHandlerBase::TransportAvailabilityInfo transports_info_1;
  transports_info_1.available_transports = {
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      FidoTransportProtocol::kNearFieldCommunication,
      FidoTransportProtocol::kBluetoothLowEnergy,
      FidoTransportProtocol::kInternal,
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
  };

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
