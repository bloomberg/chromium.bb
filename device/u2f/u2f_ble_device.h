// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_BLE_DEVICE_H_
#define DEVICE_U2F_U2F_BLE_DEVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/timer/timer.h"
#include "device/u2f/u2f_ble_connection.h"
#include "device/u2f/u2f_ble_transaction.h"
#include "device/u2f/u2f_device.h"

namespace device {

class U2fApduCommand;
class U2fBleFrame;

class U2fBleDevice : public U2fDevice {
 public:
  using FrameCallback = U2fBleTransaction::FrameCallback;
  explicit U2fBleDevice(std::string address);
  explicit U2fBleDevice(std::unique_ptr<U2fBleConnection> connection);
  ~U2fBleDevice() override;

  void Connect();
  void SendPing(std::vector<uint8_t> data, MessageCallback callback);
  static std::string GetId(base::StringPiece address);

  // U2fDevice:
  void TryWink(WinkCallback callback) override;
  std::string GetId() const override;

  U2fBleConnection::ConnectionStatusCallback
  GetConnectionStatusCallbackForTesting();
  U2fBleConnection::ReadCallback GetReadCallbackForTesting();

 protected:
  // U2fDevice:
  void DeviceTransact(std::unique_ptr<U2fApduCommand> command,
                      DeviceCallback callback) override;
  base::WeakPtr<U2fDevice> GetWeakPtr() override;

 private:
  // INIT --> BUSY --> CONNECTED --> BUSY <--> READY.
  // DEVICE_ERROR persists.
  enum class State { INIT, CONNECTED, READY, BUSY, DEVICE_ERROR };

  void Transition();

  void OnConnectionStatus(bool success);
  void OnStatusMessage(std::vector<uint8_t> data);

  void ReadControlPointLength();
  void OnReadControlPointLength(base::Optional<uint16_t> length);

  void SendPendingRequestFrame();
  void SendRequestFrame(U2fBleFrame frame, FrameCallback callback);
  void OnResponseFrame(FrameCallback callback,
                       base::Optional<U2fBleFrame> frame);

  void StartTimeout();
  void StopTimeout();
  void OnTimeout();

  State state_ = State::INIT;
  base::OneShotTimer timer_;

  std::unique_ptr<U2fBleConnection> connection_;
  uint16_t control_point_length_ = 0;

  base::queue<std::pair<U2fBleFrame, FrameCallback>> pending_frames_;
  base::Optional<U2fBleTransaction> transaction_;

  base::WeakPtrFactory<U2fBleDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(U2fBleDevice);
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_BLE_DEVICE_H_
