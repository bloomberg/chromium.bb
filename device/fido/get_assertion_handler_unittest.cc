// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "device/base/features.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/get_assertion_request_handler.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr uint8_t kBogusCredentialId[] = {0x01, 0x02, 0x03, 0x04};

using TestGetAssertionRequestCallback = test::StatusAndValuesCallbackReceiver<
    FidoReturnCode,
    base::Optional<AuthenticatorGetAssertionResponse>,
    FidoTransportProtocol>;

// Returns the set of transport protocols that are safe to test with. Because
// FidoCableDiscovery cannot be faked out, and attempting to start the real
// thing would flakily work/fail depending on the environment, avoid testing.
base::flat_set<FidoTransportProtocol> GetTestableTransportProtocols() {
  auto transports = GetAllTransportProtocols();
  transports.erase(FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
  return transports;
}

}  // namespace

class FidoGetAssertionHandlerTest : public ::testing::Test {
 public:
  void ForgeDiscoveries() {
    discovery_ = scoped_fake_discovery_factory_.ForgeNextHidDiscovery();
    ble_discovery_ = scoped_fake_discovery_factory_.ForgeNextBleDiscovery();
    nfc_discovery_ = scoped_fake_discovery_factory_.ForgeNextNfcDiscovery();
  }

  std::unique_ptr<GetAssertionRequestHandler> CreateGetAssertionHandlerU2f() {
    CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                    test_data::kClientDataHash);
    request.SetAllowList(
        {{CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)}});
    return CreateGetAssertionHandlerWithRequest(std::move(request));
  }

  std::unique_ptr<GetAssertionRequestHandler> CreateGetAssertionHandlerCtap() {
    CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                    test_data::kClientDataHash);
    request.SetAllowList({{CredentialType::kPublicKey,
                           fido_parsing_utils::Materialize(
                               test_data::kTestGetAssertionCredentialId)}});
    return CreateGetAssertionHandlerWithRequest(std::move(request));
  }

  std::unique_ptr<GetAssertionRequestHandler>
  CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest request) {
    ForgeDiscoveries();

    auto handler = std::make_unique<GetAssertionRequestHandler>(
        nullptr /* connector */, supported_transports_, std::move(request),
        get_assertion_cb_.callback());
    handler->SetPlatformAuthenticatorOrMarkUnavailable(
        CreatePlatformAuthenticator());
    return handler;
  }

  void ExpectAllowedTransportsForRequestAre(
      GetAssertionRequestHandler* request_handler,
      base::flat_set<FidoTransportProtocol> transports) {
    using Transport = FidoTransportProtocol;
    if (base::ContainsKey(transports, Transport::kUsbHumanInterfaceDevice))
      discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::ContainsKey(transports, Transport::kBluetoothLowEnergy))
      ble_discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::ContainsKey(transports, Transport::kNearFieldCommunication))
      nfc_discovery()->WaitForCallToStartAndSimulateSuccess();

    scoped_task_environment_.FastForwardUntilNoTasksRemain();
    EXPECT_FALSE(get_assertion_callback().was_called());

    if (!base::ContainsKey(transports, Transport::kUsbHumanInterfaceDevice))
      EXPECT_FALSE(discovery()->is_start_requested());
    if (!base::ContainsKey(transports, Transport::kBluetoothLowEnergy))
      EXPECT_FALSE(ble_discovery()->is_start_requested());
    if (!base::ContainsKey(transports, Transport::kNearFieldCommunication))
      EXPECT_FALSE(nfc_discovery()->is_start_requested());

    // Even with FidoTransportProtocol::kInternal allowed, unless the platform
    // authenticator factory returns a FidoAuthenticator instance (which it will
    // not be default), the transport will be marked `unavailable`.
    transports.erase(Transport::kInternal);

    EXPECT_THAT(
        request_handler->transport_availability_info().available_transports,
        ::testing::UnorderedElementsAreArray(transports));
  }

  void ExpectAllTransportsAreAllowedForRequest(
      GetAssertionRequestHandler* request_handler) {
    ExpectAllowedTransportsForRequestAre(request_handler,
                                         GetTestableTransportProtocols());
  }

  void InitFeatureListAndDisableCtapFlag() {
    scoped_feature_list_.InitAndDisableFeature(kNewCtap2Device);
  }

  test::FakeFidoDiscovery* discovery() const { return discovery_; }
  test::FakeFidoDiscovery* ble_discovery() const { return ble_discovery_; }
  test::FakeFidoDiscovery* nfc_discovery() const { return nfc_discovery_; }
  TestGetAssertionRequestCallback& get_assertion_callback() {
    return get_assertion_cb_;
  }

  void set_mock_platform_device(std::unique_ptr<MockFidoDevice> device) {
    mock_platform_device_ = std::move(device);
  }

  void set_supported_transports(
      base::flat_set<FidoTransportProtocol> transports) {
    supported_transports_ = std::move(transports);
  }

 protected:
  std::unique_ptr<FidoAuthenticator> CreatePlatformAuthenticator() {
    if (!mock_platform_device_)
      return nullptr;
    return std::make_unique<FidoDeviceAuthenticator>(
        mock_platform_device_.get());
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  test::ScopedFakeFidoDiscoveryFactory scoped_fake_discovery_factory_;
  test::FakeFidoDiscovery* discovery_;
  test::FakeFidoDiscovery* ble_discovery_;
  test::FakeFidoDiscovery* nfc_discovery_;
  std::unique_ptr<MockFidoDevice> mock_platform_device_;
  TestGetAssertionRequestCallback get_assertion_cb_;
  base::flat_set<FidoTransportProtocol> supported_transports_ =
      GetTestableTransportProtocols();
};

TEST_F(FidoGetAssertionHandlerTest, CtapRequestOnSingleDevice) {
  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  discovery()->AddDevice(std::move(device));
  get_assertion_callback().WaitForCallback();

  EXPECT_EQ(FidoReturnCode::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
  EXPECT_TRUE(request_handler->is_complete());
}

// Test a scenario where the connected authenticator is a U2F device.
TEST_F(FidoGetAssertionHandlerTest, TestU2fSign) {
  auto request_handler = CreateGetAssertionHandlerU2f();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fCheckOnlySignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);

  discovery()->AddDevice(std::move(device));
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(FidoReturnCode::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
  EXPECT_TRUE(request_handler->is_complete());
}

// Test a scenario where the connected authenticator is a U2F device and
// "WebAuthenticationCtap2" flag is not enabled.
TEST_F(FidoGetAssertionHandlerTest, TestU2fSignWithoutCtapFlag) {
  InitFeatureListAndDisableCtapFlag();
  auto request_handler = CreateGetAssertionHandlerU2f();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device0"));
  device->ExpectRequestAndRespondWith(
      test_data::kU2fCheckOnlySignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);

  discovery()->AddDevice(std::move(device));
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(FidoReturnCode::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
  EXPECT_TRUE(request_handler->is_complete());
}

TEST_F(FidoGetAssertionHandlerTest, TestIncompatibleUserVerificationSetting) {
  auto request = CtapGetAssertionRequest(test_data::kRelyingPartyId,
                                         test_data::kClientDataHash);
  request.SetUserVerification(UserVerificationRequirement::kRequired);
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutUvSupport);

  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(get_assertion_callback().was_called());
}

TEST_F(FidoGetAssertionHandlerTest,
       TestU2fSignRequestWithUserVerificationRequired) {
  auto request = CtapGetAssertionRequest(test_data::kRelyingPartyId,
                                         test_data::kClientDataHash);
  request.SetAllowList(
      {{CredentialType::kPublicKey,
        fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)}});
  request.SetUserVerification(UserVerificationRequirement::kRequired);
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(get_assertion_callback().was_called());
}

TEST_F(FidoGetAssertionHandlerTest, IncorrectRpIdHash) {
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest(
          test_data::kRelyingPartyId, test_data::kClientDataHash));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponseWithIncorrectRpIdHash);

  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(get_assertion_callback().was_called());
}

// Tests a scenario where the authenticator responds with credential ID that
// is not included in the allowed list.
TEST_F(FidoGetAssertionHandlerTest, InvalidCredential) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataHash);
  request.SetAllowList(
      {{CredentialType::kPublicKey,
        fido_parsing_utils::Materialize(test_data::kKeyHandleAlpha)}});
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  // Resident Keys must be disabled, otherwise allow list check is skipped.
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutResidentKeySupport);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(get_assertion_callback().was_called());
}

// Tests a scenario where authenticator responds without user entity in its
// response but client is expecting a resident key credential.
TEST_F(FidoGetAssertionHandlerTest, IncorrectUserEntity) {
  // Use a GetAssertion request with an empty allow list.
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest(
          test_data::kRelyingPartyId, test_data::kClientDataHash));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(get_assertion_callback().was_called());
}

TEST_F(FidoGetAssertionHandlerTest,
       AllTransportsAllowedIfAllowCredentialsListUndefined) {
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest(
          test_data::kRelyingPartyId, test_data::kClientDataHash));
  ExpectAllTransportsAreAllowedForRequest(request_handler.get());
}

TEST_F(FidoGetAssertionHandlerTest,
       AllTransportsAllowedIfAllowCredentialsListIsEmpty) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataHash);
  request.SetAllowList({});

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  ExpectAllTransportsAreAllowedForRequest(request_handler.get());
}

TEST_F(FidoGetAssertionHandlerTest,
       AllTransportsAllowedIfHasAllowedCredentialWithEmptyTransportsList) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataHash);
  request.SetAllowList({
      {CredentialType::kPublicKey,
       fido_parsing_utils::Materialize(
           test_data::kTestGetAssertionCredentialId),
       {FidoTransportProtocol::kBluetoothLowEnergy}},
      {CredentialType::kPublicKey,
       fido_parsing_utils::Materialize(kBogusCredentialId)},
  });

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  ExpectAllTransportsAreAllowedForRequest(request_handler.get());
}

TEST_F(FidoGetAssertionHandlerTest,
       AllowedTransportsAreUnionOfTransportsLists) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataHash);
  request.SetAllowList({
      {CredentialType::kPublicKey,
       fido_parsing_utils::Materialize(
           test_data::kTestGetAssertionCredentialId),
       {FidoTransportProtocol::kBluetoothLowEnergy}},
      {CredentialType::kPublicKey,
       fido_parsing_utils::Materialize(kBogusCredentialId),
       {FidoTransportProtocol::kInternal,
        FidoTransportProtocol::kNearFieldCommunication}},
  });

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  ExpectAllowedTransportsForRequestAre(
      request_handler.get(), {FidoTransportProtocol::kBluetoothLowEnergy,
                              FidoTransportProtocol::kInternal,
                              FidoTransportProtocol::kNearFieldCommunication});
}

TEST_F(FidoGetAssertionHandlerTest, SupportedTransportsAreOnlyBleAndNfc) {
  const base::flat_set<FidoTransportProtocol> kBleAndNfc = {
      FidoTransportProtocol::kBluetoothLowEnergy,
      FidoTransportProtocol::kNearFieldCommunication,
  };

  set_supported_transports(kBleAndNfc);

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest(
          test_data::kRelyingPartyId, test_data::kClientDataHash));
  ExpectAllowedTransportsForRequestAre(request_handler.get(), kBleAndNfc);
}

TEST_F(FidoGetAssertionHandlerTest, SupportedTransportsAreOnlyUsbAndInternal) {
  const base::flat_set<FidoTransportProtocol> kUsbAndInternal = {
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
      FidoTransportProtocol::kInternal,
  };

  set_supported_transports(kUsbAndInternal);
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest(
          test_data::kRelyingPartyId, test_data::kClientDataHash));
  ExpectAllowedTransportsForRequestAre(request_handler.get(), kUsbAndInternal);
}

TEST_F(FidoGetAssertionHandlerTest, SuccessWithOnlyUsbTransportAllowed) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataHash);
  request.SetAllowList({
      {CredentialType::kPublicKey,
       fido_parsing_utils::Materialize(
           test_data::kTestGetAssertionCredentialId),
       {FidoTransportProtocol::kUsbHumanInterfaceDevice}},
  });

  set_supported_transports({FidoTransportProtocol::kUsbHumanInterfaceDevice});

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  get_assertion_callback().WaitForCallback();

  EXPECT_EQ(FidoReturnCode::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(
          FidoTransportProtocol::kUsbHumanInterfaceDevice));
}

TEST_F(FidoGetAssertionHandlerTest, SuccessWithOnlyBleTransportAllowed) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataHash);
  request.SetAllowList({
      {CredentialType::kPublicKey,
       fido_parsing_utils::Materialize(
           test_data::kTestGetAssertionCredentialId),
       {FidoTransportProtocol::kBluetoothLowEnergy}},
  });

  set_supported_transports({FidoTransportProtocol::kBluetoothLowEnergy});

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->SetDeviceTransport(FidoTransportProtocol::kBluetoothLowEnergy);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);
  ble_discovery()->WaitForCallToStartAndSimulateSuccess();
  ble_discovery()->AddDevice(std::move(device));

  get_assertion_callback().WaitForCallback();

  EXPECT_EQ(FidoReturnCode::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(
          FidoTransportProtocol::kBluetoothLowEnergy));
}

TEST_F(FidoGetAssertionHandlerTest, SuccessWithOnlyNfcTransportAllowed) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataHash);
  request.SetAllowList({
      {CredentialType::kPublicKey,
       fido_parsing_utils::Materialize(
           test_data::kTestGetAssertionCredentialId),
       {FidoTransportProtocol::kNearFieldCommunication}},
  });

  set_supported_transports({FidoTransportProtocol::kNearFieldCommunication});

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->SetDeviceTransport(FidoTransportProtocol::kNearFieldCommunication);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);
  nfc_discovery()->WaitForCallToStartAndSimulateSuccess();
  nfc_discovery()->AddDevice(std::move(device));

  get_assertion_callback().WaitForCallback();

  EXPECT_EQ(FidoReturnCode::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(
          FidoTransportProtocol::kNearFieldCommunication));
}

TEST_F(FidoGetAssertionHandlerTest, SuccessWithOnlyInternalTransportAllowed) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataHash);
  request.SetAllowList({
      {CredentialType::kPublicKey,
       fido_parsing_utils::Materialize(
           test_data::kTestGetAssertionCredentialId),
       {FidoTransportProtocol::kInternal}},
  });

  set_supported_transports({FidoTransportProtocol::kInternal});

  auto device = MockFidoDevice::MakeCtap(
      ReadCTAPGetInfoResponse(test_data::kTestGetInfoResponsePlatformDevice));
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device0"));
  device->SetDeviceTransport(FidoTransportProtocol::kInternal);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);
  set_mock_platform_device(std::move(device));

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  get_assertion_callback().WaitForCallback();

  EXPECT_EQ(FidoReturnCode::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(FidoTransportProtocol::kInternal));
}

// Tests a scenario where authenticator of incorrect transport type was used to
// conduct CTAP GetAssertion call.
//
// TODO(engedy): This should not happen, instead |allowCredentials| should be
// filtered to only contain items compatible with the transport actually used to
// talk to the authenticator.
TEST_F(FidoGetAssertionHandlerTest, IncorrectTransportType) {
  // GetAssertion request that expects GetAssertion call for credential
  // |CredentialType::kPublicKey| to be signed with Cable authenticator.
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataHash);
  request.SetAllowList({
      {CredentialType::kPublicKey,
       fido_parsing_utils::Materialize(
           test_data::kTestGetAssertionCredentialId),
       {FidoTransportProtocol::kBluetoothLowEnergy}},
      {CredentialType::kPublicKey,
       fido_parsing_utils::Materialize(kBogusCredentialId),
       {FidoTransportProtocol::kUsbHumanInterfaceDevice}},
  });
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  // Since transport type of |device| is different from what the relying party
  // defined in |request| above, this request should fail.
  device->SetDeviceTransport(FidoTransportProtocol::kUsbHumanInterfaceDevice);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(get_assertion_callback().was_called());
}

}  // namespace device
