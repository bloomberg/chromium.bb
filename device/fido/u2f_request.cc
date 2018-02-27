// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_request.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/fido/u2f_apdu_command.h"
#include "device/fido/u2f_ble_discovery.h"
#include "services/service_manager/public/cpp/connector.h"

// HID is not supported on Android.
#if !defined(OS_ANDROID)
#include "device/fido/u2f_hid_discovery.h"
#endif  // !defined(OS_ANDROID)

namespace device {

U2fRequest::U2fRequest(service_manager::Connector* connector,
                       const base::flat_set<U2fTransportProtocol>& protocols,
                       std::vector<uint8_t> application_parameter,
                       std::vector<uint8_t> challenge_digest,
                       std::vector<std::vector<uint8_t>> registered_keys)
    : state_(State::INIT),
      application_parameter_(application_parameter),
      challenge_digest_(challenge_digest),
      registered_keys_(registered_keys),
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
    state_ = State::IDLE;
    for (auto& discovery : discoveries_)
      discovery->Start();
  }
}

void U2fRequest::SetDiscoveriesForTesting(
    std::vector<std::unique_ptr<U2fDiscovery>> discoveries) {
  discoveries_ = std::move(discoveries);
  for (auto& discovery : discoveries_)
    discovery->AddObserver(this);
}

// static
const std::vector<uint8_t>& U2fRequest::GetBogusApplicationParameter() {
  static const std::vector<uint8_t> kBogusAppParam(32, 0x41);
  return kBogusAppParam;
}

// static
const std::vector<uint8_t>& U2fRequest::GetBogusChallenge() {
  static const std::vector<uint8_t> kBogusChallenge(32, 0x42);
  return kBogusChallenge;
}

// static
std::unique_ptr<U2fApduCommand> U2fRequest::GetU2fVersionApduCommand(
    bool is_legacy_version) {
  return is_legacy_version ? U2fApduCommand::CreateLegacyVersion()
                           : U2fApduCommand::CreateVersion();
}

std::unique_ptr<U2fApduCommand> U2fRequest::GetU2fSignApduCommand(
    const std::vector<uint8_t>& key_handle,
    bool is_check_only_sign) const {
  return U2fApduCommand::CreateSign(application_parameter_, challenge_digest_,
                                    key_handle, is_check_only_sign);
}

std::unique_ptr<U2fApduCommand> U2fRequest::GetU2fRegisterApduCommand(
    bool is_individual_attestation) const {
  return U2fApduCommand::CreateRegister(
      application_parameter_, challenge_digest_, is_individual_attestation);
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
  if (success) {
    // The discovery might know about devices that have already been added to
    // the system. Add them to the |devices_| list if we don't already know
    // about them. This case could happen if a DeviceAdded() event is emitted
    // before DiscoveryStarted() is invoked. In that case both the U2fRequest
    // and the U2fDiscovery already know about the just added device.
    std::set<std::string> current_device_ids;
    for (const auto* device : attempted_devices_)
      current_device_ids.insert(device->GetId());
    if (current_device_)
      current_device_ids.insert(current_device_->GetId());
    for (const auto* device : devices_)
      current_device_ids.insert(device->GetId());

    auto new_devices = discovery->GetDevices();
    std::copy_if(
        new_devices.begin(), new_devices.end(), std::back_inserter(devices_),
        [&current_device_ids](U2fDevice* device) {
          return !base::ContainsKey(current_device_ids, device->GetId());
        });
  }

  started_count_++;

  if ((state_ == State::IDLE || state_ == State::OFF) &&
      (success || started_count_ == discoveries_.size())) {
    state_ = State::IDLE;
    Transition();
  }
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

}  // namespace device
