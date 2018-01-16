// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_ble_device.h"

#include "base/bind.h"
#include "base/strings/string_piece.h"
#include "device/u2f/u2f_apdu_command.h"
#include "device/u2f/u2f_ble_frames.h"
#include "device/u2f/u2f_ble_transaction.h"

namespace device {

U2fBleDevice::U2fBleDevice(std::string address) : weak_factory_(this) {
  connection_ = std::make_unique<U2fBleConnection>(
      std::move(address),
      base::BindRepeating(&U2fBleDevice::OnConnectionStatus,
                          base::Unretained(this)),
      base::BindRepeating(&U2fBleDevice::OnStatusMessage,
                          base::Unretained(this)));
}

U2fBleDevice::U2fBleDevice(std::unique_ptr<U2fBleConnection> connection)
    : connection_(std::move(connection)), weak_factory_(this) {}

U2fBleDevice::~U2fBleDevice() = default;

void U2fBleDevice::Connect() {
  if (state_ != State::INIT)
    return;

  StartTimeout();
  state_ = State::BUSY;
  connection_->Connect();
}

void U2fBleDevice::SendPing(std::vector<uint8_t> data,
                            MessageCallback callback) {
  pending_frames_.emplace(
      U2fBleFrame(U2fCommandType::CMD_PING, std::move(data)),
      base::BindOnce(
          [](MessageCallback callback, base::Optional<U2fBleFrame> frame) {
            std::move(callback).Run(
                frame ? U2fReturnCode::SUCCESS : U2fReturnCode::FAILURE,
                frame ? frame->data() : std::vector<uint8_t>());
          },
          std::move(callback)));
  Transition();
}

// static
std::string U2fBleDevice::GetId(base::StringPiece address) {
  return std::string("ble:").append(address.begin(), address.end());
}

void U2fBleDevice::TryWink(WinkCallback callback) {
  // U2F over BLE does not support winking.
  std::move(callback).Run();
}

std::string U2fBleDevice::GetId() const {
  return GetId(connection_->address());
}

U2fBleConnection::ConnectionStatusCallback
U2fBleDevice::GetConnectionStatusCallbackForTesting() {
  return base::BindRepeating(&U2fBleDevice::OnConnectionStatus,
                             base::Unretained(this));
}

U2fBleConnection::ReadCallback U2fBleDevice::GetReadCallbackForTesting() {
  return base::BindRepeating(&U2fBleDevice::OnStatusMessage,
                             base::Unretained(this));
}

void U2fBleDevice::DeviceTransact(std::unique_ptr<U2fApduCommand> command,
                                  DeviceCallback callback) {
  pending_frames_.emplace(
      U2fBleFrame(U2fCommandType::CMD_MSG, command->GetEncodedCommand()),
      base::BindOnce(
          [](DeviceCallback callback, base::Optional<U2fBleFrame> frame) {
            std::move(callback).Run(
                frame.has_value(),
                frame ? U2fApduResponse::CreateFromMessage(frame->data())
                      : nullptr);
          },
          std::move(callback)));
  Transition();
}

base::WeakPtr<U2fDevice> U2fBleDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void U2fBleDevice::Transition() {
  switch (state_) {
    case State::INIT:
      Connect();
      break;
    case State::CONNECTED:
      StartTimeout();
      state_ = State::BUSY;
      connection_->ReadControlPointLength(base::BindOnce(
          &U2fBleDevice::OnReadControlPointLength, base::Unretained(this)));
      break;
    case State::READY:
      if (!pending_frames_.empty()) {
        U2fBleFrame frame;
        FrameCallback callback;
        std::tie(frame, callback) = std::move(pending_frames_.front());
        pending_frames_.pop();
        SendRequestFrame(std::move(frame), std::move(callback));
      }
      break;
    case State::BUSY:
      break;
    case State::DEVICE_ERROR:
      auto self = GetWeakPtr();
      // Executing callbacks may free |this|. Check |self| first.
      while (self && !pending_frames_.empty()) {
        // Respond to any pending frames.
        FrameCallback cb = std::move(pending_frames_.front().second);
        pending_frames_.pop();
        std::move(cb).Run(base::nullopt);
      }
      break;
  }
}

void U2fBleDevice::OnConnectionStatus(bool success) {
  StopTimeout();
  state_ = success ? State::CONNECTED : State::DEVICE_ERROR;
  Transition();
}

void U2fBleDevice::OnReadControlPointLength(base::Optional<uint16_t> length) {
  StopTimeout();
  if (length) {
    transaction_.emplace(connection_.get(), *length);
    state_ = State::READY;
  } else {
    state_ = State::DEVICE_ERROR;
  }
  Transition();
}

void U2fBleDevice::OnStatusMessage(std::vector<uint8_t> data) {
  if (transaction_)
    transaction_->OnResponseFragment(std::move(data));
}

void U2fBleDevice::SendRequestFrame(U2fBleFrame frame, FrameCallback callback) {
  state_ = State::BUSY;
  transaction_->WriteRequestFrame(
      std::move(frame),
      base::BindOnce(&U2fBleDevice::OnResponseFrame, base::Unretained(this),
                     std::move(callback)));
}

void U2fBleDevice::OnResponseFrame(FrameCallback callback,
                                   base::Optional<U2fBleFrame> frame) {
  state_ = frame ? State::READY : State::DEVICE_ERROR;
  auto self = GetWeakPtr();
  std::move(callback).Run(std::move(frame));
  // Executing callbacks may free |this|. Check |self| first.
  if (self)
    Transition();
}

void U2fBleDevice::StartTimeout() {
  timer_.Start(FROM_HERE, U2fDevice::kDeviceTimeout, this,
               &U2fBleDevice::OnTimeout);
}

void U2fBleDevice::StopTimeout() {
  timer_.Stop();
}

void U2fBleDevice::OnTimeout() {
  state_ = State::DEVICE_ERROR;
}

}  // namespace device
