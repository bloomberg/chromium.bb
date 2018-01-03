// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_hid_device.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "crypto/random.h"
#include "device/u2f/u2f_apdu_command.h"
#include "device/u2f/u2f_command_type.h"
#include "device/u2f/u2f_message.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {

namespace switches {
static constexpr char kEnableU2fHidTest[] = "enable-u2f-hid-tests";
}  // namespace switches

namespace {
// U2F devices only provide a single report so specify a report ID of 0 here.
static constexpr uint8_t kReportId = 0x00;
}  // namespace

U2fHidDevice::U2fHidDevice(device::mojom::HidDeviceInfoPtr device_info,
                           device::mojom::HidManager* hid_manager)
    : U2fDevice(),
      state_(State::INIT),
      hid_manager_(hid_manager),
      device_info_(std::move(device_info)),
      weak_factory_(this) {}

U2fHidDevice::~U2fHidDevice() = default;

void U2fHidDevice::DeviceTransact(std::unique_ptr<U2fApduCommand> command,
                                  DeviceCallback callback) {
  Transition(std::move(command), std::move(callback));
}

void U2fHidDevice::Transition(std::unique_ptr<U2fApduCommand> command,
                              DeviceCallback callback) {
  // This adapter is needed to support the calls to ArmTimeout(). However, it is
  // still guaranteed that |callback| will only be invoked once.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  switch (state_) {
    case State::INIT:
      state_ = State::BUSY;
      ArmTimeout(repeating_callback);
      Connect(base::BindOnce(&U2fHidDevice::OnConnect,
                             weak_factory_.GetWeakPtr(), std::move(command),
                             repeating_callback));
      break;
    case State::CONNECTED:
      state_ = State::BUSY;
      ArmTimeout(repeating_callback);
      AllocateChannel(std::move(command), repeating_callback);
      break;
    case State::IDLE: {
      state_ = State::BUSY;
      std::unique_ptr<U2fMessage> msg = U2fMessage::Create(
          channel_id_, U2fCommandType::CMD_MSG, command->GetEncodedCommand());

      ArmTimeout(repeating_callback);
      // Write message to the device
      WriteMessage(
          std::move(msg), true,
          base::BindOnce(&U2fHidDevice::MessageReceived,
                         weak_factory_.GetWeakPtr(), repeating_callback));
      break;
    }
    case State::BUSY:
      pending_transactions_.emplace(std::move(command), repeating_callback);
      break;
    case State::DEVICE_ERROR:
    default:
      base::WeakPtr<U2fHidDevice> self = weak_factory_.GetWeakPtr();
      repeating_callback.Run(false, nullptr);

      // Executing callbacks may free |this|. Check |self| first.
      while (self && !pending_transactions_.empty()) {
        // Respond to any pending requests
        DeviceCallback pending_cb =
            std::move(pending_transactions_.front().second);
        pending_transactions_.pop();
        std::move(pending_cb).Run(false, nullptr);
      }
      break;
  }
}

void U2fHidDevice::Connect(ConnectCallback callback) {
  DCHECK(hid_manager_);
  hid_manager_->Connect(device_info_->guid, std::move(callback));
}

void U2fHidDevice::OnConnect(std::unique_ptr<U2fApduCommand> command,
                             DeviceCallback callback,
                             device::mojom::HidConnectionPtr connection) {
  if (state_ == State::DEVICE_ERROR)
    return;
  timeout_callback_.Cancel();

  if (connection) {
    connection_ = std::move(connection);
    state_ = State::CONNECTED;
  } else {
    state_ = State::DEVICE_ERROR;
  }
  Transition(std::move(command), std::move(callback));
}

void U2fHidDevice::AllocateChannel(std::unique_ptr<U2fApduCommand> command,
                                   DeviceCallback callback) {
  // Send random nonce to device to verify received message
  std::vector<uint8_t> nonce(8);
  crypto::RandBytes(nonce.data(), nonce.size());
  std::unique_ptr<U2fMessage> message =
      U2fMessage::Create(channel_id_, U2fCommandType::CMD_INIT, nonce);

  WriteMessage(std::move(message), true,
               base::BindOnce(&U2fHidDevice::OnAllocateChannel,
                              weak_factory_.GetWeakPtr(), nonce,
                              std::move(command), std::move(callback)));
}

void U2fHidDevice::OnAllocateChannel(std::vector<uint8_t> nonce,
                                     std::unique_ptr<U2fApduCommand> command,
                                     DeviceCallback callback,
                                     bool success,
                                     std::unique_ptr<U2fMessage> message) {
  if (state_ == State::DEVICE_ERROR)
    return;
  timeout_callback_.Cancel();

  if (!success || !message) {
    state_ = State::DEVICE_ERROR;
    Transition(nullptr, std::move(callback));
    return;
  }

  // Channel allocation response is defined as:
  // 0: 8 byte nonce
  // 8: 4 byte channel id
  // 12: Protocol version id
  // 13: Major device version
  // 14: Minor device version
  // 15: Build device version
  // 16: Capabilities
  std::vector<uint8_t> payload = message->GetMessagePayload();
  if (payload.size() != 17) {
    state_ = State::DEVICE_ERROR;
    Transition(nullptr, std::move(callback));
    return;
  }
  auto received_nonce = base::make_span(payload).first(8);
  // Received a broadcast message for a different client. Disregard and continue
  // reading.
  if (base::make_span(nonce) != received_nonce) {
    auto repeating_callback =
        base::AdaptCallbackForRepeating(std::move(callback));
    ArmTimeout(repeating_callback);
    ReadMessage(base::BindOnce(&U2fHidDevice::OnAllocateChannel,
                               weak_factory_.GetWeakPtr(), nonce,
                               std::move(command), repeating_callback));

    return;
  }

  size_t index = 8;
  channel_id_ = payload[index++] << 24;
  channel_id_ |= payload[index++] << 16;
  channel_id_ |= payload[index++] << 8;
  channel_id_ |= payload[index++];
  capabilities_ = payload[16];
  state_ = State::IDLE;
  Transition(std::move(command), std::move(callback));
}

void U2fHidDevice::WriteMessage(std::unique_ptr<U2fMessage> message,
                                bool response_expected,
                                U2fHidMessageCallback callback) {
  if (!connection_ || !message || message->NumPackets() == 0) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  const auto& packet = message->PopNextPacket();
  connection_->Write(
      kReportId, packet,
      base::BindOnce(&U2fHidDevice::PacketWritten, weak_factory_.GetWeakPtr(),
                     std::move(message), true, std::move(callback)));
}

void U2fHidDevice::PacketWritten(std::unique_ptr<U2fMessage> message,
                                 bool response_expected,
                                 U2fHidMessageCallback callback,
                                 bool success) {
  if (success && message->NumPackets() > 0) {
    WriteMessage(std::move(message), response_expected, std::move(callback));
  } else if (success && response_expected) {
    ReadMessage(std::move(callback));
  } else {
    std::move(callback).Run(success, nullptr);
  }
}

void U2fHidDevice::ReadMessage(U2fHidMessageCallback callback) {
  if (!connection_) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  connection_->Read(base::BindOnce(
      &U2fHidDevice::OnRead, weak_factory_.GetWeakPtr(), std::move(callback)));
}

void U2fHidDevice::OnRead(U2fHidMessageCallback callback,
                          bool success,
                          uint8_t report_id,
                          const base::Optional<std::vector<uint8_t>>& buf) {
  if (!success) {
    std::move(callback).Run(success, nullptr);
    return;
  }

  DCHECK(buf);

  std::unique_ptr<U2fMessage> read_message =
      U2fMessage::CreateFromSerializedData(*buf);

  if (!read_message) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  // Received a message from a different channel, so try again
  if (channel_id_ != read_message->channel_id()) {
    connection_->Read(base::BindOnce(&U2fHidDevice::OnRead,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(callback)));
    return;
  }

  if (read_message->MessageComplete()) {
    std::move(callback).Run(success, std::move(read_message));
    return;
  }

  // Continue reading additional packets
  connection_->Read(base::BindOnce(
      &U2fHidDevice::OnReadContinuation, weak_factory_.GetWeakPtr(),
      std::move(read_message), std::move(callback)));
}

void U2fHidDevice::OnReadContinuation(
    std::unique_ptr<U2fMessage> message,
    U2fHidMessageCallback callback,
    bool success,
    uint8_t report_id,
    const base::Optional<std::vector<uint8_t>>& buf) {
  if (!success) {
    std::move(callback).Run(success, nullptr);
    return;
  }

  DCHECK(buf);
  message->AddContinuationPacket(*buf);
  if (message->MessageComplete()) {
    std::move(callback).Run(success, std::move(message));
    return;
  }
  connection_->Read(base::BindOnce(&U2fHidDevice::OnReadContinuation,
                                   weak_factory_.GetWeakPtr(),
                                   std::move(message), std::move(callback)));
}

void U2fHidDevice::MessageReceived(DeviceCallback callback,
                                   bool success,
                                   std::unique_ptr<U2fMessage> message) {
  if (state_ == State::DEVICE_ERROR)
    return;
  timeout_callback_.Cancel();

  if (!success) {
    state_ = State::DEVICE_ERROR;
    Transition(nullptr, std::move(callback));
    return;
  }

  std::unique_ptr<U2fApduResponse> response = nullptr;
  if (message)
    response = U2fApduResponse::CreateFromMessage(message->GetMessagePayload());
  state_ = State::IDLE;
  base::WeakPtr<U2fHidDevice> self = weak_factory_.GetWeakPtr();
  std::move(callback).Run(success, std::move(response));

  // Executing |callback| may have freed |this|. Check |self| first.
  if (self && !pending_transactions_.empty()) {
    // If any transactions were queued, process the first one
    std::unique_ptr<U2fApduCommand> pending_cmd =
        std::move(pending_transactions_.front().first);
    DeviceCallback pending_cb = std::move(pending_transactions_.front().second);
    pending_transactions_.pop();
    Transition(std::move(pending_cmd), std::move(pending_cb));
  }
}

void U2fHidDevice::TryWink(WinkCallback callback) {
  // Only try to wink if device claims support
  if (!(capabilities_ & kWinkCapability) || state_ != State::IDLE) {
    std::move(callback).Run();
    return;
  }

  std::unique_ptr<U2fMessage> wink_message = U2fMessage::Create(
      channel_id_, U2fCommandType::CMD_WINK, std::vector<uint8_t>());
  WriteMessage(std::move(wink_message), true,
               base::BindOnce(&U2fHidDevice::OnWink, weak_factory_.GetWeakPtr(),
                              std::move(callback)));
}

void U2fHidDevice::OnWink(WinkCallback callback,
                          bool success,
                          std::unique_ptr<U2fMessage> response) {
  std::move(callback).Run();
}

void U2fHidDevice::ArmTimeout(DeviceCallback callback) {
  DCHECK(timeout_callback_.IsCancelled());
  timeout_callback_.Reset(base::BindOnce(&U2fHidDevice::OnTimeout,
                                         weak_factory_.GetWeakPtr(),
                                         std::move(callback)));
  // Setup timeout task for 3 seconds
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, timeout_callback_.callback(), kDeviceTimeout);
}

void U2fHidDevice::OnTimeout(DeviceCallback callback) {
  state_ = State::DEVICE_ERROR;
  Transition(nullptr, std::move(callback));
}

std::string U2fHidDevice::GetId() const {
  return GetIdForDevice(*device_info_);
}

// static
std::string U2fHidDevice::GetIdForDevice(
    const device::mojom::HidDeviceInfo& device_info) {
  return "hid:" + device_info.guid;
}

// static
bool U2fHidDevice::IsTestEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableU2fHidTest);
}

base::WeakPtr<U2fDevice> U2fHidDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
