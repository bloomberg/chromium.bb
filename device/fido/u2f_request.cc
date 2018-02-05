// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_request.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/fido/u2f_ble_discovery.h"
#include "services/service_manager/public/cpp/connector.h"

// HID is not supported on Android.
#if !defined(OS_ANDROID)
#include "device/fido/u2f_hid_discovery.h"
#endif  // !defined(OS_ANDROID)

namespace device {

U2fRequest::U2fRequest(std::string relying_party_id,
                       service_manager::Connector* connector,
                       const base::flat_set<U2fTransportProtocol>& protocols)
    : state_(State::INIT),
      relying_party_id_(std::move(relying_party_id)),
      weak_factory_(this) {
  for (const auto protocol : protocols) {
    std::unique_ptr<U2fDiscovery> discovery;
    switch (protocol) {
      case U2fTransportProtocol::kUsbHumanInterfaceDevice:
#if !defined(OS_ANDROID)
        DCHECK(connector);
        discovery = std::make_unique<U2fHidDiscovery>(connector);
#endif  // !defined(OS_ANDROID)
        break;
      case U2fTransportProtocol::kBluetoothLowEnergy:
        discovery = std::make_unique<U2fBleDiscovery>();
        break;
    }

    discovery->AddObserver(this);
    discoveries_.push_back(std::move(discovery));
  }
}

U2fRequest::~U2fRequest() {
  for (auto& discovery : discoveries_)
    discovery->RemoveObserver(this);
}

void U2fRequest::Start() {
  if (state_ == State::INIT) {
    state_ = State::BUSY;
    for (auto& discovery : discoveries_)
      discovery->Start();
  }
}

U2fDiscovery* U2fRequest::SetDiscoveryForTesting(
    std::unique_ptr<U2fDiscovery> discovery) {
  discoveries_.clear();
  discovery->AddObserver(this);
  discoveries_.push_back(std::move(discovery));
  return discoveries_.back().get();
}

std::vector<std::unique_ptr<U2fDiscovery>>&
U2fRequest::SetDiscoveriesForTesting(
    std::unique_ptr<U2fDiscovery> discovery_1,
    std::unique_ptr<U2fDiscovery> discovery_2) {
  discoveries_.clear();
  discovery_1->AddObserver(this);
  discovery_2->AddObserver(this);
  discoveries_.push_back(std::move(discovery_1));
  discoveries_.push_back(std::move(discovery_2));
  return discoveries_;
}

void U2fRequest::SetDiscoveriesForTesting(
    std::vector<std::unique_ptr<U2fDiscovery>> discoveries) {
  discoveries_ = std::move(discoveries);
  for (auto& discovery : discoveries_)
    discovery->AddObserver(this);
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
      break;
    default:
      break;
  }
}

void U2fRequest::DiscoveryStarted(U2fDiscovery* discovery, bool success) {
  // The discovery might know about devices that have already been added to the
  // system. Add them to the |devices_| list.
  auto devices = discovery->GetDevices();
  std::move(devices.begin(), devices.end(), std::back_inserter(devices_));

  if (++started_count_ < discoveries_.size())
    return;

  state_ = State::IDLE;
  Transition();
}

void U2fRequest::DiscoveryStopped(U2fDiscovery* discovery, bool success) {}

void U2fRequest::DeviceAdded(U2fDiscovery* discovery, U2fDevice* device) {
  devices_.push_back(device);

  // Start the state machine if this is the only device
  if (state_ == State::OFF) {
    state_ = State::IDLE;
    delay_callback_.Cancel();
    Transition();
  }
}

void U2fRequest::DeviceRemoved(U2fDiscovery* discovery, U2fDevice* device) {
  const std::string device_id = device->GetId();
  auto device_id_eq = [&device_id](const U2fDevice* this_device) {
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
  if (current_device_) {
    attempted_devices_.push_back(current_device_);
    current_device_ = nullptr;
  }

  // If there is an additional device on device list, make it active.
  // Otherwise, if all devices have been tried, move attempted devices back to
  // the main device list.
  if (devices_.size() > 0) {
    current_device_ = devices_.front();
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

// static
const std::vector<uint8_t>& U2fRequest::GetBogusAppParam() {
  static const std::vector<uint8_t> kBogusAppParam(32, 0x41);
  return kBogusAppParam;
}

// static
const std::vector<uint8_t>& U2fRequest::GetBogusChallenge() {
  static const std::vector<uint8_t> kBogusChallenge(32, 0x42);
  return kBogusChallenge;
}

}  // namespace device
