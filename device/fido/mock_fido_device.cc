// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mock_fido_device.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/apdu/apdu_response.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_response_test_data.h"
#include "device/fido/u2f_parsing_utils.h"

namespace device {

MATCHER_P(IsCtap2Command, expected_command, "") {
  return !arg.empty() && (arg[0] == static_cast<uint8_t>(expected_command));
}

MockFidoDevice::MockFidoDevice() : weak_factory_(this) {}
MockFidoDevice::~MockFidoDevice() = default;

void MockFidoDevice::TryWink(WinkCallback cb) {
  TryWinkRef(cb);
}

void MockFidoDevice::DeviceTransact(std::vector<uint8_t> command,
                                    DeviceCallback cb) {
  DeviceTransactPtr(command, cb);
}

// static
void MockFidoDevice::NotSatisfied(const std::vector<uint8_t>& command,
                                  DeviceCallback& cb) {
  std::move(cb).Run(apdu::ApduResponse(
                        std::vector<uint8_t>(),
                        apdu::ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED)
                        .GetEncodedResponse());
}

// static
void MockFidoDevice::WrongData(const std::vector<uint8_t>& command,
                               DeviceCallback& cb) {
  std::move(cb).Run(
      apdu::ApduResponse(std::vector<uint8_t>(),
                         apdu::ApduResponse::Status::SW_WRONG_DATA)
          .GetEncodedResponse());
}

// static
void MockFidoDevice::NoErrorGetInfo(const std::vector<uint8_t>& command,
                                    DeviceCallback& cb) {
  std::move(cb).Run(std::vector<uint8_t>(
      std::begin(test_data::kTestAuthenticatorGetInfoResponse),
      std::end(test_data::kTestAuthenticatorGetInfoResponse)));
}

// static
void MockFidoDevice::CtapDeviceError(const std::vector<uint8_t>& command,
                                     DeviceCallback& cb) {
  std::move(cb).Run(base::nullopt);
}

// static
void MockFidoDevice::NoErrorMakeCredential(const std::vector<uint8_t>& command,
                                           DeviceCallback& cb) {
  std::move(cb).Run(
      std::vector<uint8_t>(std::begin(test_data::kTestMakeCredentialResponse),
                           std::end(test_data::kTestMakeCredentialResponse)));
}

// static
void MockFidoDevice::NoErrorSign(const std::vector<uint8_t>& command,
                                 DeviceCallback& cb) {
  std::move(cb).Run(
      apdu::ApduResponse(
          std::vector<uint8_t>(std::begin(test_data::kTestU2fSignResponse),
                               std::end(test_data::kTestU2fSignResponse)),
          apdu::ApduResponse::Status::SW_NO_ERROR)
          .GetEncodedResponse());
}

// static
void MockFidoDevice::NoErrorRegister(const std::vector<uint8_t>& command,
                                     DeviceCallback& cb) {
  std::move(cb).Run(
      apdu::ApduResponse(
          std::vector<uint8_t>(std::begin(test_data::kTestU2fRegisterResponse),
                               std::end(test_data::kTestU2fRegisterResponse)),
          apdu::ApduResponse::Status::SW_NO_ERROR)
          .GetEncodedResponse());
}

// static
void MockFidoDevice::NoErrorVersion(const std::vector<uint8_t>& command,
                                    DeviceCallback& cb) {
  std::move(cb).Run(
      apdu::ApduResponse(std::vector<uint8_t>(kU2fVersionResponse.cbegin(),
                                              kU2fVersionResponse.cend()),
                         apdu::ApduResponse::Status::SW_NO_ERROR)
          .GetEncodedResponse());
}

// static
void MockFidoDevice::SignWithCorruptedResponse(
    const std::vector<uint8_t>& command,
    DeviceCallback& cb) {
  std::move(cb).Run(
      apdu::ApduResponse(
          std::vector<uint8_t>(
              std::begin(test_data::kTestCorruptedU2fSignResponse),
              std::end(test_data::kTestCorruptedU2fSignResponse)),
          apdu::ApduResponse::Status::SW_NO_ERROR)
          .GetEncodedResponse());
}

// static
void MockFidoDevice::WinkDoNothing(WinkCallback& cb) {
  std::move(cb).Run();
}

void MockFidoDevice::ExpectWinkedAtLeastOnce() {
  EXPECT_CALL(*this, TryWinkRef(::testing::_)).Times(::testing::AtLeast(1));
}

void MockFidoDevice::ExpectCtap2CommandAndRespondWith(
    CtapRequestCommand command,
    base::Optional<base::span<const uint8_t>> response,
    base::TimeDelta delay) {
  auto data = u2f_parsing_utils::MaterializeOrNull(response);
  auto send_response = [ data(std::move(data)), delay ](DeviceCallback & cb) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(cb), std::move(data)), delay);
  };
  EXPECT_CALL(*this, DeviceTransactPtr(IsCtap2Command(command), ::testing::_))
      .WillOnce(::testing::WithArg<1>(::testing::Invoke(send_response)));
}

void MockFidoDevice::ExpectCtap2CommandWithoutResponse(
    CtapRequestCommand command) {
  EXPECT_CALL(*this, DeviceTransactPtr(IsCtap2Command(command), ::testing::_));
}

base::WeakPtr<FidoDevice> MockFidoDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
