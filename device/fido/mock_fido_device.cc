// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mock_fido_device.h"

#include <utility>

#include "components/apdu/apdu_response.h"
#include "device/fido/fido_constants.h"
#include "device/fido/u2f_response_test_data.h"

namespace device {

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

base::WeakPtr<FidoDevice> MockFidoDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
