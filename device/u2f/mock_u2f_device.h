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

  MockU2fDevice();
  ~MockU2fDevice() override;

  // GMock cannot mock a method taking a move-only type.
  // TODO(crbug.com/729950): Remove these workarounds once support for move-only
  // types is added to GMock.
  MOCK_METHOD1(TryWinkRef, void(WinkCallback& cb));
  void TryWink(WinkCallback cb) override;

  MOCK_CONST_METHOD0(GetId, std::string(void));
  // GMock cannot mock a method taking a move-only type.
  // TODO(crbug.com/729950): Remove these workarounds once support for move-only
  // types is added to GMock.
  MOCK_METHOD2(DeviceTransactPtr,
               void(U2fApduCommand* command, DeviceCallback& cb));
  void DeviceTransact(std::unique_ptr<U2fApduCommand> command,
                      DeviceCallback cb) override;
  base::WeakPtr<U2fDevice> GetWeakPtr() override;
  static void TransactNoError(std::unique_ptr<U2fApduCommand> command,
                              DeviceCallback cb);
  static void NotSatisfied(U2fApduCommand* cmd, DeviceCallback& cb);
  static void WrongData(U2fApduCommand* cmd, DeviceCallback& cb);
  static void NoErrorSign(U2fApduCommand* cmd, DeviceCallback& cb);
  static void NoErrorRegister(U2fApduCommand* cmd, DeviceCallback& cb);
  static void SignWithCorruptedResponse(U2fApduCommand* cmd,
                                        DeviceCallback& cb);
  static void WinkDoNothing(WinkCallback& cb);

 private:
  base::WeakPtrFactory<U2fDevice> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_U2F_MOCK_U2F_DEVICE_H_
