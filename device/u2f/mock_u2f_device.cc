// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_u2f_device.h"

namespace device {

MockU2fDevice::MockU2fDevice() {}

MockU2fDevice::~MockU2fDevice() {}

void MockU2fDevice::DeviceTransact(std::unique_ptr<U2fApduCommand> command,
                                   const DeviceCallback& cb) {
  DeviceTransactPtr(command.get(), cb);
}

// static
void MockU2fDevice::NotSatisfied(U2fApduCommand* cmd,
                                 const DeviceCallback& cb) {
  cb.Run(true, base::MakeUnique<U2fApduResponse>(
                   std::vector<uint8_t>(),
                   U2fApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED));
}

// static
void MockU2fDevice::WrongData(U2fApduCommand* cmd, const DeviceCallback& cb) {
  cb.Run(true,
         base::MakeUnique<U2fApduResponse>(
             std::vector<uint8_t>(), U2fApduResponse::Status::SW_WRONG_DATA));
}

// static
void MockU2fDevice::NoErrorSign(U2fApduCommand* cmd, const DeviceCallback& cb) {
  cb.Run(true, base::MakeUnique<U2fApduResponse>(
                   std::vector<uint8_t>({kSign}),
                   U2fApduResponse::Status::SW_NO_ERROR));
}

// static
void MockU2fDevice::NoErrorRegister(U2fApduCommand* cmd,
                                    const DeviceCallback& cb) {
  cb.Run(true, base::MakeUnique<U2fApduResponse>(
                   std::vector<uint8_t>({kRegister}),
                   U2fApduResponse::Status::SW_NO_ERROR));
}

// static
void MockU2fDevice::WinkDoNothing(const WinkCallback& cb) {
  cb.Run();
}

}  // namespace device
