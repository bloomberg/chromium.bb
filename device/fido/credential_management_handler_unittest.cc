// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/credential_management_handler.h"

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "device/fido/credential_management.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/scoped_virtual_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using test::ScopedVirtualFidoDevice;

constexpr char kPIN[] = "1234";
constexpr uint8_t kCredentialID[] = {0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
                                     0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa};
constexpr char kRPID[] = "example.com";
constexpr uint8_t kUserID[] = {0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
                               0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1};
constexpr char kUserName[] = "alice@example.com";
constexpr char kUserDisplayName[] = "Alice Example <alice@example.com>";

class TestObserver : public FidoRequestHandlerBase::Observer {
 public:
  TestObserver() {}
  ~TestObserver() override {}

 private:
  // FidoRequestHandlerBase::Observer:
  void OnTransportAvailabilityEnumerated(
      FidoRequestHandlerBase::TransportAvailabilityInfo data) override {}
  bool EmbedderControlsAuthenticatorDispatch(
      const FidoAuthenticator&) override {
    return false;
  }
  void BluetoothAdapterPowerChanged(bool is_powered_on) override {}
  void FidoAuthenticatorAdded(const FidoAuthenticator& authenticator) override {
  }
  void FidoAuthenticatorRemoved(base::StringPiece device_id) override {}
  void FidoAuthenticatorIdChanged(base::StringPiece old_authenticator_id,
                                  std::string new_authenticator_id) override {}
  void FidoAuthenticatorPairingModeChanged(base::StringPiece authenticator_id,
                                           bool is_in_pairing_mode) override {}
  bool SupportsPIN() const override { return true; }
  void CollectPIN(
      base::Optional<int> attempts,
      base::OnceCallback<void(std::string)> provide_pin_cb) override {
    std::move(provide_pin_cb).Run(kPIN);
  }
  void FinishCollectPIN() override {}
  void SetMightCreateResidentCredential(bool v) override {}
};

class TestDelegate : public CredentialManagementHandler::Delegate {
 public:
  TestDelegate() {}
  ~TestDelegate() {}

  test::StatusAndValuesCallbackReceiver<CtapDeviceResponseCode, size_t, size_t>
      on_credential_metadata;
  test::StatusAndValuesCallbackReceiver<
      std::vector<AggregatedEnumerateCredentialsResponse>>
      on_credentials_enumerated;
  test::ValueCallbackReceiver<FidoReturnCode> on_error;

 private:
  // CredentialManagementHandler::Delegate
  void OnCredentialMetadata(size_t num_existing,
                            size_t num_remaining) override {
    on_credential_metadata.callback().Run(CtapDeviceResponseCode::kSuccess,
                                          num_existing, num_remaining);
  }
  void OnCredentialsEnumerated(
      std::vector<AggregatedEnumerateCredentialsResponse> credentials)
      override {
    on_credentials_enumerated.callback().Run(std::move(credentials));
  }
  void OnError(FidoReturnCode error) override {
    on_error.callback().Run(error);
  }
};

TEST(CredentialManagementHandlerTest, Test) {
  base::test::ScopedTaskEnvironment scoped_task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);

  ScopedVirtualFidoDevice virtual_device;
  VirtualCtap2Device::Config ctap_config;
  ctap_config.pin_support = true;
  ctap_config.resident_key_support = true;
  ctap_config.credential_management_support = true;
  ctap_config.resident_credential_storage = 100;
  virtual_device.SetCtap2Config(ctap_config);
  virtual_device.SetSupportedProtocol(device::ProtocolVersion::kCtap);
  virtual_device.mutable_state()->pin = kPIN;
  virtual_device.mutable_state()->retries = 8;
  ASSERT_TRUE(virtual_device.mutable_state()->InjectResidentKey(
      kCredentialID, kRPID, kUserID, kUserName, kUserDisplayName));

  TestDelegate delegate;
  CredentialManagementHandler handler(
      /*connector=*/nullptr, {FidoTransportProtocol::kUsbHumanInterfaceDevice},
      &delegate);
  TestObserver observer;
  handler.set_observer(&observer);

  scoped_task_environment.FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(delegate.on_error.was_called())
      << "error " << static_cast<int>(delegate.on_error.value());
  EXPECT_TRUE(delegate.on_credential_metadata.was_called());
  EXPECT_EQ(delegate.on_credential_metadata.value<0>(), 1u);
  EXPECT_EQ(delegate.on_credential_metadata.value<1>(), 99u);

  // TODO(martinkr): This should be true but is not yet implemented.
  EXPECT_FALSE(delegate.on_credentials_enumerated.was_called());
}

}  // namespace
}  // namespace device
