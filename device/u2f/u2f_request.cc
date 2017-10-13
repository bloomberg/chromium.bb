// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_request.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace device {

U2fRequest::U2fRequest(std::vector<std::unique_ptr<U2fDiscovery>> discoveries,
                       ResponseCallback cb)
    : state_(State::INIT),
      discoveries_(std::move(discoveries)),
      cb_(std::move(cb)),
      weak_factory_(this) {
  for (auto& discovery : discoveries_) {
    discovery->SetDelegate(weak_factory_.GetWeakPtr());
  }
}

U2fRequest::~U2fRequest() = default;

void U2fRequest::Start() {
  if (state_ == State::INIT) {
    state_ = State::BUSY;
    for (auto& discovery : discoveries_) {
      discovery->Start();
    }
  }
}

void U2fRequest::Transition() {
  switch (state_) {
    case State::IDLE:
      IterateDevice();
      if (!current_device_) {
        // No devices available
        state_ = State::OFF;
        break;
      }
      state_ = State::WINK;
      current_device_->TryWink(
          base::Bind(&U2fRequest::Transition, weak_factory_.GetWeakPtr()));
      break;
    case State::WINK:
      state_ = State::BUSY;
      TryDevice();
    default:
      break;
  }
}

void U2fRequest::OnStarted(bool success) {
  if (++started_count_ < discoveries_.size())
    return;

  state_ = State::IDLE;
  Transition();
}

void U2fRequest::OnStopped(bool success) {}

void U2fRequest::OnDeviceAdded(std::unique_ptr<U2fDevice> device) {
  devices_.push_back(std::move(device));

  // Start the state machine if this is the only device
  if (state_ == State::OFF) {
    state_ = State::IDLE;
    delay_callback_.Cancel();
    Transition();
  }
}

void U2fRequest::OnDeviceRemoved(base::StringPiece device_id) {
  auto device_id_eq =
      [&device_id](const std::unique_ptr<U2fDevice>& this_device) {
        return device_id == this_device->GetId();
      };

  // Check if the active device was removed
  if (current_device_ && device_id_eq(current_device_)) {
    current_device_ = nullptr;
    state_ = State::IDLE;
    Transition();
    return;
  }

  // Remove the device if it exists in either device list
  devices_.remove_if(device_id_eq);
  attempted_devices_.remove_if(device_id_eq);
}

void U2fRequest::IterateDevice() {
  // Move active device to attempted device list
  if (current_device_)
    attempted_devices_.push_back(std::move(current_device_));

  // If there is an additional device on device list, make it active.
  // Otherwise, if all devices have been tried, move attempted devices back to
  // the main device list.
  if (devices_.size() > 0) {
    current_device_ = std::move(devices_.front());
    devices_.pop_front();
  } else if (attempted_devices_.size() > 0) {
    devices_ = std::move(attempted_devices_);
    // After trying every device, wait 200ms before trying again
    delay_callback_.Reset(
        base::Bind(&U2fRequest::OnWaitComplete, weak_factory_.GetWeakPtr()));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, delay_callback_.callback(),
        base::TimeDelta::FromMilliseconds(200));
  }
}

void U2fRequest::OnWaitComplete() {
  state_ = State::IDLE;
  Transition();
}

}  // namespace device
