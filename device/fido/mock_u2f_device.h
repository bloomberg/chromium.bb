// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MOCK_U2F_DEVICE_H_
#define DEVICE_FIDO_MOCK_U2F_DEVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "device/fido/u2f_device.h"
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
               void(std::vector<uint8_t> command, DeviceCallback& cb));
  void DeviceTransact(std::vector<uint8_t> command, DeviceCallback cb) override;
  base::WeakPtr<U2fDevice> GetWeakPtr() override;
  static void TransactNoError(const std::vector<uint8_t>& command,
                              DeviceCallback cb);
  static void NotSatisfied(const std::vector<uint8_t>& command,
                           DeviceCallback& cb);
  static void WrongData(const std::vector<uint8_t>& command,
                        DeviceCallback& cb);
  static void NoErrorSign(const std::vector<uint8_t>& command,
                          DeviceCallback& cb);
  static void NoErrorRegister(const std::vector<uint8_t>& command,
                              DeviceCallback& cb);
  static void SignWithCorruptedResponse(const std::vector<uint8_t>& command,
                                        DeviceCallback& cb);
  static void WinkDoNothing(WinkCallback& cb);

 private:
  base::WeakPtrFactory<U2fDevice> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_FIDO_MOCK_U2F_DEVICE_H_
