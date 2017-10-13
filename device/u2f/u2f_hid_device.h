// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_HID_DEVICE_H_
#define DEVICE_U2F_U2F_HID_DEVICE_H_

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "device/hid/public/interfaces/hid.mojom.h"
#include "device/u2f/u2f_device.h"

namespace device {

class U2fMessage;

class U2fHidDevice : public U2fDevice {
 public:
  U2fHidDevice(device::mojom::HidDeviceInfoPtr device_info,
               device::mojom::HidManager* hid_manager);

  ~U2fHidDevice() final;

  // Send a U2f command to this device
  void DeviceTransact(std::unique_ptr<U2fApduCommand> command,
                      const DeviceCallback& callback) final;
  // Send a wink command if supported
  void TryWink(const WinkCallback& callback) final;
  // Use a string identifier to compare to other devices
  std::string GetId() const final;
  // Get a string identifier for a given device info
  static std::string GetIdForDevice(
      const device::mojom::HidDeviceInfo& device_info);
  // Command line flag to enable tests on actual U2f HID hardware
  static bool IsTestEnabled();

 private:
  FRIEND_TEST_ALL_PREFIXES(U2fHidDeviceTest, TestConnectionFailure);
  FRIEND_TEST_ALL_PREFIXES(U2fHidDeviceTest, TestDeviceError);

  // Internal state machine states
  enum class State { INIT, CONNECTED, BUSY, IDLE, DEVICE_ERROR };

  using U2fHidMessageCallback =
      base::OnceCallback<void(bool, std::unique_ptr<U2fMessage>)>;
  using ConnectCallback = device::mojom::HidManager::ConnectCallback;

  // Open a connection to this device
  void Connect(ConnectCallback callback);
  void OnConnect(std::unique_ptr<U2fApduCommand> command,
                 const DeviceCallback& callback,
                 device::mojom::HidConnectionPtr connection);
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
              uint8_t report_id,
              const base::Optional<std::vector<uint8_t>>& buf);
  void OnReadContinuation(std::unique_ptr<U2fMessage> message,
                          U2fHidMessageCallback callback,
                          bool success,
                          uint8_t report_id,
                          const base::Optional<std::vector<uint8_t>>& buf);
  void OnWink(const WinkCallback& callback,
              bool success,
              std::unique_ptr<U2fMessage> response);
  void ArmTimeout(const DeviceCallback& callback);
  void OnTimeout(const DeviceCallback& callback);
  void OnDeviceTransact(bool success,
                        std::unique_ptr<U2fApduResponse> response);
  base::WeakPtr<U2fDevice> GetWeakPtr() override;

  State state_;
  base::CancelableClosure timeout_callback_;
  std::list<std::pair<std::unique_ptr<U2fApduCommand>, DeviceCallback>>
      pending_transactions_;

  // All the U2fHidDevice instances are owned by U2fRequest. So it is safe to
  // let the U2fHidDevice share the device::mojo::HidManager raw pointer from
  // U2fRequest.
  device::mojom::HidManager* hid_manager_;
  device::mojom::HidDeviceInfoPtr device_info_;
  device::mojom::HidConnectionPtr connection_;
  base::WeakPtrFactory<U2fHidDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(U2fHidDevice);
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_HID_DEVICE_H_
