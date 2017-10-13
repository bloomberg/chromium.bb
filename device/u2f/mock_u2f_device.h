// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_MOCK_U2F_DEVICE_H_
#define DEVICE_U2F_MOCK_U2F_DEVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "device/u2f/u2f_apdu_command.h"
#include "device/u2f/u2f_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockU2fDevice : public U2fDevice {
 public:
  static constexpr uint8_t kSign = 0x1;
  static constexpr uint8_t kRegister = 0x5;

  MockU2fDevice();
  ~MockU2fDevice() override;

  MOCK_METHOD1(TryWink, void(const WinkCallback& cb));
  MOCK_CONST_METHOD0(GetId, std::string(void));
  // GMock cannot mock a method taking a std::unique_ptr<T>
  MOCK_METHOD2(DeviceTransactPtr,
               void(U2fApduCommand* command, const DeviceCallback& cb));
  void DeviceTransact(std::unique_ptr<U2fApduCommand> command,
                      const DeviceCallback& cb) override;
  base::WeakPtr<U2fDevice> GetWeakPtr() override;
  static void TransactNoError(std::unique_ptr<U2fApduCommand> command,
                              const DeviceCallback& cb);
  static void NotSatisfied(U2fApduCommand* cmd, const DeviceCallback& cb);
  static void WrongData(U2fApduCommand* cmd, const DeviceCallback& cb);
  static void NoErrorSign(U2fApduCommand* cmd, const DeviceCallback& cb);
  static void NoErrorRegister(U2fApduCommand* cmd, const DeviceCallback& cb);
  static void WinkDoNothing(const WinkCallback& cb);

 private:
  base::WeakPtrFactory<U2fDevice> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_U2F_MOCK_U2F_DEVICE_H_
