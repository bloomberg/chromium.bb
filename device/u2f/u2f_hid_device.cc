// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "u2f_hid_device.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "crypto/random.h"
#include "device/base/device_client.h"
#include "device/hid/hid_connection.h"
#include "u2f_apdu_command.h"
#include "u2f_message.h"

namespace device {

namespace switches {
static constexpr char kEnableU2fHidTest[] = "enable-u2f-hid-tests";
}  // namespace switches

U2fHidDevice::U2fHidDevice(scoped_refptr<HidDeviceInfo> device_info)
    : U2fDevice(),
      state_(State::INIT),
      device_info_(device_info),
      weak_factory_(this) {
  channel_id_ = kBroadcastChannel;
}

U2fHidDevice::~U2fHidDevice() {
  // Cleanup connection
  if (connection_)
    connection_->Close();
}

void U2fHidDevice::DeviceTransact(std::unique_ptr<U2fApduCommand> command,
                                  const DeviceCallback& callback) {
  Transition(std::move(command), callback);
}

void U2fHidDevice::Transition(std::unique_ptr<U2fApduCommand> command,
                              const DeviceCallback& callback) {
  switch (state_) {
    case State::INIT:
      state_ = State::BUSY;
      Connect(base::Bind(&U2fHidDevice::OnConnect, weak_factory_.GetWeakPtr(),
                         base::Passed(&command), callback));
      break;
    case State::CONNECTED:
      state_ = State::BUSY;
      AllocateChannel(std::move(command), callback);
      break;
    case State::IDLE: {
      state_ = State::BUSY;
      std::unique_ptr<U2fMessage> msg = U2fMessage::Create(
          channel_id_, U2fMessage::Type::CMD_MSG, command->GetEncodedCommand());
      WriteMessage(std::move(msg), true,
                   base::Bind(&U2fHidDevice::MessageReceived,
                              weak_factory_.GetWeakPtr(), callback));
      break;
    }
    case State::BUSY:
      pending_transactions_.push_back({std::move(command), callback});
      break;
    case State::DEVICE_ERROR:
    default:
      base::WeakPtr<U2fHidDevice> self = weak_factory_.GetWeakPtr();
      callback.Run(false, nullptr);
      // Executing callbacks may free |this|. Check |self| first.
      while (self && !pending_transactions_.empty()) {
        // Respond to any pending requests
        DeviceCallback pending_cb = pending_transactions_.front().second;
        pending_transactions_.pop_front();
        pending_cb.Run(false, nullptr);
      }
      break;
  }
}

void U2fHidDevice::Connect(const HidService::ConnectCallback& callback) {
  HidService* hid_service = DeviceClient::Get()->GetHidService();

  hid_service->Connect(device_info_->device_id(), callback);
}

void U2fHidDevice::OnConnect(std::unique_ptr<U2fApduCommand> command,
                             const DeviceCallback& callback,
                             scoped_refptr<HidConnection> connection) {
  if (connection) {
    connection_ = connection;
    state_ = State::CONNECTED;
  } else {
    state_ = State::DEVICE_ERROR;
  }
  Transition(std::move(command), callback);
}

void U2fHidDevice::AllocateChannel(std::unique_ptr<U2fApduCommand> command,
                                   const DeviceCallback& callback) {
  // Send random nonce to device to verify received message
  std::vector<uint8_t> nonce(8);
  crypto::RandBytes(nonce.data(), nonce.size());
  std::unique_ptr<U2fMessage> message =
      U2fMessage::Create(channel_id_, U2fMessage::Type::CMD_INIT, nonce);

  WriteMessage(
      std::move(message), true,
      base::Bind(&U2fHidDevice::OnAllocateChannel, weak_factory_.GetWeakPtr(),
                 nonce, base::Passed(&command), callback));
}

void U2fHidDevice::OnAllocateChannel(std::vector<uint8_t> nonce,
                                     std::unique_ptr<U2fApduCommand> command,
                                     const DeviceCallback& callback,
                                     bool success,
                                     std::unique_ptr<U2fMessage> message) {
  if (!success || !message) {
    state_ = State::DEVICE_ERROR;
    Transition(nullptr, callback);
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
    Transition(nullptr, callback);
    return;
  }

  std::vector<uint8_t> received_nonce(std::begin(payload),
                                      std::begin(payload) + 8);
  if (nonce != received_nonce) {
    state_ = State::DEVICE_ERROR;
    Transition(nullptr, callback);
    return;
  }

  size_t index = 8;
  channel_id_ = payload[index++] << 24;
  channel_id_ |= payload[index++] << 16;
  channel_id_ |= payload[index++] << 8;
  channel_id_ |= payload[index++];
  capabilities_ = payload[16];

  state_ = State::IDLE;
  Transition(std::move(command), callback);
}

void U2fHidDevice::WriteMessage(std::unique_ptr<U2fMessage> message,
                                bool response_expected,
                                U2fHidMessageCallback callback) {
  if (!connection_ || !message || message->NumPackets() == 0) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  scoped_refptr<net::IOBufferWithSize> buffer = message->PopNextPacket();

  connection_->Write(
      buffer, buffer->size(),
      base::Bind(&U2fHidDevice::PacketWritten, weak_factory_.GetWeakPtr(),
                 base::Passed(&message), true, base::Passed(&callback)));
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

  connection_->Read(base::Bind(&U2fHidDevice::OnRead,
                               weak_factory_.GetWeakPtr(),
                               base::Passed(&callback)));
}

void U2fHidDevice::OnRead(U2fHidMessageCallback callback,
                          bool success,
                          scoped_refptr<net::IOBuffer> buf,
                          size_t size) {
  if (!success || !buf) {
    std::move(callback).Run(success, nullptr);
    return;
  }

  std::vector<uint8_t> read_buffer(buf->data(), buf->data() + size);
  std::unique_ptr<U2fMessage> read_message =
      U2fMessage::CreateFromSerializedData(read_buffer);

  if (!read_message) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  // Received a message from a different channel, so try again
  if (channel_id_ != read_message->channel_id()) {
    connection_->Read(base::Bind(&U2fHidDevice::OnRead,
                                 weak_factory_.GetWeakPtr(),
                                 base::Passed(&callback)));
    return;
  }

  if (read_message->MessageComplete()) {
    std::move(callback).Run(success, std::move(read_message));
    return;
  }

  // Continue reading additional packets
  connection_->Read(
      base::Bind(&U2fHidDevice::OnReadContinuation, weak_factory_.GetWeakPtr(),
                 base::Passed(&read_message), base::Passed(&callback)));
}

void U2fHidDevice::OnReadContinuation(std::unique_ptr<U2fMessage> message,
                                      U2fHidMessageCallback callback,
                                      bool success,
                                      scoped_refptr<net::IOBuffer> buf,
                                      size_t size) {
  if (!success || !buf) {
    std::move(callback).Run(success, nullptr);
    return;
  }

  std::vector<uint8_t> read_buffer(buf->data(), buf->data() + size);
  message->AddContinuationPacket(read_buffer);
  if (message->MessageComplete()) {
    std::move(callback).Run(success, std::move(message));
    return;
  }
  connection_->Read(
      base::Bind(&U2fHidDevice::OnReadContinuation, weak_factory_.GetWeakPtr(),
                 base::Passed(&message), base::Passed(&callback)));
}

void U2fHidDevice::MessageReceived(const DeviceCallback& callback,
                                   bool success,
                                   std::unique_ptr<U2fMessage> message) {
  if (!success) {
    state_ = State::DEVICE_ERROR;
    Transition(nullptr, callback);
    return;
  }
  std::unique_ptr<U2fApduResponse> response = nullptr;
  if (message)
    response = U2fApduResponse::CreateFromMessage(message->GetMessagePayload());
  state_ = State::IDLE;
  base::WeakPtr<U2fHidDevice> self = weak_factory_.GetWeakPtr();
  callback.Run(success, std::move(response));

  // Executing |callback| may have freed |this|. Check |self| first.
  if (self && !pending_transactions_.empty()) {
    // If any transactions were queued, process the first one
    std::unique_ptr<U2fApduCommand> pending_cmd =
        std::move(pending_transactions_.front().first);
    DeviceCallback pending_cb = pending_transactions_.front().second;
    pending_transactions_.pop_front();
    Transition(std::move(pending_cmd), pending_cb);
  }
}

void U2fHidDevice::TryWink(const WinkCallback& callback) {
  // Only try to wink if device claims support
  if (!(capabilities_ & kWinkCapability) || state_ != State::IDLE) {
    callback.Run();
    return;
  }

  std::unique_ptr<U2fMessage> wink_message = U2fMessage::Create(
      channel_id_, U2fMessage::Type::CMD_WINK, std::vector<uint8_t>());
  WriteMessage(
      std::move(wink_message), true,
      base::Bind(&U2fHidDevice::OnWink, weak_factory_.GetWeakPtr(), callback));
}

void U2fHidDevice::OnWink(const WinkCallback& callback,
                          bool success,
                          std::unique_ptr<U2fMessage> response) {
  callback.Run();
}

std::string U2fHidDevice::GetId() {
  std::ostringstream id("hid:");
  id << device_info_->device_id();
  return id.str();
}

// static
bool U2fHidDevice::IsTestEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableU2fHidTest);
}

}  // namespace device
