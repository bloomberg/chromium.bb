// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_HID_DEVICE_H_
#define DEVICE_U2F_U2F_HID_DEVICE_H_

#include <list>

#include "device/hid/hid_service.h"
#include "u2f_device.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace device {

class U2fMessage;
class HidConnection;
class HidDeviceInfo;

class U2fHidDevice : public U2fDevice {
 public:
  U2fHidDevice(scoped_refptr<HidDeviceInfo>);
  ~U2fHidDevice();

  // Send a U2f command to this device
  void DeviceTransact(std::unique_ptr<U2fApduCommand> command,
                      const DeviceCallback& callback) final;
  // Send a wink command if supported
  void TryWink(const WinkCallback& callback) final;
  // Use a string identifier to compare to other devices
  std::string GetId() final;
  // Command line flag to enable tests on actual U2f HID hardware
  static bool IsTestEnabled();

 private:
  FRIEND_TEST_ALL_PREFIXES(U2fHidDeviceTest, TestConnectionFailure);
  FRIEND_TEST_ALL_PREFIXES(U2fHidDeviceTest, TestDeviceError);

  // Internal state machine states
  enum class State { INIT, CONNECTED, BUSY, IDLE, DEVICE_ERROR };

  using U2fHidMessageCallback =
      base::OnceCallback<void(bool, std::unique_ptr<U2fMessage>)>;

  // Open a connection to this device
  void Connect(const HidService::ConnectCallback& callback);
  void OnConnect(std::unique_ptr<U2fApduCommand> command,
                 const DeviceCallback& callback,
                 scoped_refptr<HidConnection> connection);
  // Ask device to allocate a unique channel id for this connection
  void AllocateChannel(std::unique_ptr<U2fApduCommand> command,
                       const DeviceCallback& callback);
  void OnAllocateChannel(std::vector<uint8_t> nonce,
                         std::unique_ptr<U2fApduCommand> command,
                         const DeviceCallback& callback,
                         bool success,
                         std::unique_ptr<U2fMessage> message);
  void Transition(std::unique_ptr<U2fApduCommand> command,
                  const DeviceCallback& callback);
  // Write all message packets to device, and read response if expected
  void WriteMessage(std::unique_ptr<U2fMessage> message,
                    bool response_expected,
                    U2fHidMessageCallback callback);
  void PacketWritten(std::unique_ptr<U2fMessage> message,
                     bool response_expected,
                     U2fHidMessageCallback callback,
                     bool success);
  // Read all response message packets from device
  void ReadMessage(U2fHidMessageCallback callback);
  void MessageReceived(const DeviceCallback& callback,
                       bool success,
                       std::unique_ptr<U2fMessage> message);
  void OnRead(U2fHidMessageCallback callback,
              bool success,
              scoped_refptr<net::IOBuffer> buf,
              size_t size);
  void OnReadContinuation(std::unique_ptr<U2fMessage> message,
                          U2fHidMessageCallback,
                          bool success,
                          scoped_refptr<net::IOBuffer> buf,
                          size_t size);
  void OnWink(const WinkCallback& callback,
              bool success,
              std::unique_ptr<U2fMessage> response);

  State state_;
  std::list<std::pair<std::unique_ptr<U2fApduCommand>, DeviceCallback>>
      pending_transactions_;
  scoped_refptr<HidDeviceInfo> device_info_;
  scoped_refptr<HidConnection> connection_;
  base::WeakPtrFactory<U2fHidDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(U2fHidDevice);
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_HID_DEVICE_H_
