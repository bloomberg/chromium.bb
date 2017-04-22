// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "u2f_request.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/base/device_client.h"
#include "u2f_hid_device.h"

namespace device {

U2fRequest::U2fRequest(const ResponseCallback& cb)
    : state_(State::INIT),
      cb_(cb),
      hid_service_observer_(this),
      weak_factory_(this) {
  filter_.SetUsagePage(0xf1d0);
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

void U2fRequest::Start() {
  if (state_ == State::INIT) {
    state_ = State::BUSY;
    Enumerate();
  }
}

void U2fRequest::Enumerate() {
  HidService* hid_service = DeviceClient::Get()->GetHidService();
  DCHECK(hid_service);
  hid_service_observer_.Add(hid_service);
  hid_service->GetDevices(
      base::Bind(&U2fRequest::OnEnumerate, weak_factory_.GetWeakPtr()));
}

void U2fRequest::OnEnumerate(
    const std::vector<scoped_refptr<HidDeviceInfo>>& devices) {
  for (auto device_info : devices) {
    if (filter_.Matches(device_info))
      devices_.push_back(base::MakeUnique<U2fHidDevice>(device_info));
  }

  state_ = State::IDLE;
  Transition();
}

void U2fRequest::OnDeviceAdded(scoped_refptr<HidDeviceInfo> device_info) {
  // Ignore non-U2F devices
  if (!filter_.Matches(device_info))
    return;

  auto device = base::MakeUnique<U2fHidDevice>(device_info);
  AddDevice(std::move(device));
}

void U2fRequest::OnDeviceRemoved(scoped_refptr<HidDeviceInfo> device_info) {
  // Ignore non-U2F devices
  if (!filter_.Matches(device_info))
    return;

  auto device = base::MakeUnique<U2fHidDevice>(device_info);

  // Check if the active device was removed
  if (current_device_ && current_device_->GetId() == device->GetId()) {
    current_device_ = nullptr;
    state_ = State::IDLE;
    Transition();
    return;
  }

  // Remove the device if it exists in either device list
  devices_.remove_if([&device](const std::unique_ptr<U2fDevice>& this_device) {
    return this_device->GetId() == device->GetId();
  });
  attempted_devices_.remove_if(
      [&device](const std::unique_ptr<U2fDevice>& this_device) {
        return this_device->GetId() == device->GetId();
      });
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

void U2fRequest::AddDevice(std::unique_ptr<U2fDevice> device) {
  devices_.push_back(std::move(device));

  // Start the state machine if this is the only device
  if (state_ == State::OFF) {
    state_ = State::IDLE;
    delay_callback_.Cancel();
    Transition();
  }
}

void U2fRequest::AddDeviceForTesting(std::unique_ptr<U2fDevice> device) {
  AddDevice(std::move(device));
}

U2fRequest::~U2fRequest() {}

}  // namespace device
