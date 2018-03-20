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
#include "components/apdu/apdu_command.h"
#include "components/apdu/apdu_response.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

U2fRequest::U2fRequest(service_manager::Connector* connector,
                       const base::flat_set<U2fTransportProtocol>& transports,
                       std::vector<uint8_t> application_parameter,
                       std::vector<uint8_t> challenge_digest,
                       std::vector<std::vector<uint8_t>> registered_keys)
    : state_(State::INIT),
      application_parameter_(application_parameter),
      challenge_digest_(challenge_digest),
      registered_keys_(registered_keys),
      weak_factory_(this) {
  for (const auto transport : transports) {
    auto discovery = FidoDiscovery::Create(transport, connector);
    if (discovery == nullptr) {
      // This can occur in tests when a ScopedVirtualU2fDevice is in effect and
      // non-HID transports are configured.
      continue;
    }
    discovery->set_observer(this);
    discoveries_.push_back(std::move(discovery));
  }
}

U2fRequest::~U2fRequest() = default;

void U2fRequest::Start() {
  if (state_ == State::INIT) {
    state_ = State::IDLE;
    for (auto& discovery : discoveries_)
      discovery->Start();
  }
}

// static
std::vector<uint8_t> U2fRequest::GetBogusRegisterCommand() {
  apdu::ApduCommand command;
  std::vector<uint8_t> data(kBogusChallenge.cbegin(), kBogusChallenge.cend());
  data.insert(data.end(), kBogusAppParam.cbegin(), kBogusAppParam.cend());
  command.set_ins(base::strict_cast<uint8_t>(U2fApduInstruction::kRegister));
  command.set_p1(kP1TupRequiredConsumed);
  command.set_data(data);
  return command.GetEncodedCommand();
}

// static
std::vector<uint8_t> U2fRequest::GetU2fVersionApduCommand(
    bool is_legacy_version) {
  apdu::ApduCommand command;
  command.set_ins(base::strict_cast<uint8_t>(U2fApduInstruction::kVersion));
  // Set maximum expected response length to maximum length possible.
  command.set_response_length(kU2fMaxResponseSize);
  // Early U2F drafts defined the U2F version command a format
  // incompatible with ISO 7816-4, so 2 additional 0x0 bytes are necessary.
  // https://fidoalliance.org/specs/fido-u2f-v1.1-id-20160915/fido-u2f-raw-message-formats-v1.1-id-20160915.html#implementation-considerations
  auto version_cmd = command.GetEncodedCommand();
  if (is_legacy_version)
    version_cmd.insert(version_cmd.end(), kLegacyVersionSuffix.cbegin(),
                       kLegacyVersionSuffix.cend());

  return version_cmd;
}

base::Optional<std::vector<uint8_t>> U2fRequest::GetU2fSignApduCommand(
    const std::vector<uint8_t>& application_parameter,
    const std::vector<uint8_t>& key_handle,
    bool is_check_only_sign) const {
  if (application_parameter.size() != kU2fParameterLength ||
      challenge_digest_.size() != kU2fParameterLength ||
      key_handle.size() > kMaxKeyHandleLength) {
    return base::nullopt;
  }
  apdu::ApduCommand command;
  std::vector<uint8_t> data(challenge_digest_.begin(), challenge_digest_.end());
  data.insert(data.end(), application_parameter.begin(),
              application_parameter.end());
  data.push_back(static_cast<uint8_t>(key_handle.size()));
  data.insert(data.end(), key_handle.begin(), key_handle.end());
  command.set_ins(base::strict_cast<uint8_t>(U2fApduInstruction::kSign));
  command.set_p1(is_check_only_sign ? kP1CheckOnly : kP1TupRequiredConsumed);
  command.set_data(data);
  return command.GetEncodedCommand();
}

base::Optional<std::vector<uint8_t>> U2fRequest::GetU2fRegisterApduCommand(
    bool is_individual_attestation) const {
  if (application_parameter_.size() != kU2fParameterLength ||
      challenge_digest_.size() != kU2fParameterLength) {
    return base::nullopt;
  }
  apdu::ApduCommand command;
  std::vector<uint8_t> data(challenge_digest_.begin(), challenge_digest_.end());
  data.insert(data.end(), application_parameter_.begin(),
              application_parameter_.end());
  command.set_ins(base::strict_cast<uint8_t>(U2fApduInstruction::kRegister));
  command.set_p1(kP1TupRequiredConsumed |
                 (is_individual_attestation ? kP1IndividualAttestation : 0));
  command.set_data(data);
  return command.GetEncodedCommand();
}

void U2fRequest::Transition() {
  switch (state_) {
    case State::IDLE:
      IterateDevice();
      if (!current_device_) {
        // No devices available.
        state_ = State::OFF;
        break;
      }
      state_ = State::WINK;
      current_device_->TryWink(
          base::BindOnce(&U2fRequest::Transition, weak_factory_.GetWeakPtr()));
      break;
    case State::WINK:
      state_ = State::BUSY;
      TryDevice();
      break;
    default:
      break;
  }
}

void U2fRequest::InitiateDeviceTransaction(
    base::Optional<std::vector<uint8_t>> cmd,
    FidoDevice::DeviceCallback callback) {
  if (!cmd) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  current_device_->DeviceTransact(std::move(*cmd), std::move(callback));
}

void U2fRequest::OnDeviceVersionRequest(
    VersionCallback callback,
    base::WeakPtr<FidoDevice> device,
    bool legacy,
    base::Optional<std::vector<uint8_t>> response) {
  const auto apdu_response =
      response ? apdu::ApduResponse::CreateFromMessage(std::move(*response))
               : base::nullopt;
  if (apdu_response &&
      apdu_response->status() == apdu::ApduResponse::Status::SW_NO_ERROR &&
      std::equal(apdu_response->data().cbegin(), apdu_response->data().cend(),
                 kU2fVersionResponse.cbegin(), kU2fVersionResponse.cend())) {
    std::move(callback).Run(ProtocolVersion::kU2f);
  } else if (!legacy) {
    // Standard GetVersion failed, attempt legacy GetVersion command.
    device->DeviceTransact(
        GetU2fVersionApduCommand(true),
        base::BindOnce(&U2fRequest::OnDeviceVersionRequest,
                       weak_factory_.GetWeakPtr(), std::move(callback), device,
                       true /* legacy */));
  } else {
    std::move(callback).Run(ProtocolVersion::kUnknown);
  }
}

void U2fRequest::DiscoveryStarted(FidoDiscovery* discovery, bool success) {
#if DCHECK_IS_ON()
  if (success) {
    // FidoDiscovery::Observer::DeviceAdded should have been already dispatched
    // for each of devices already known by |discovery|, so we should never
    // learn anything new here.
    std::set<std::string> device_ids_known_to_request;
    for (const auto* device : attempted_devices_)
      device_ids_known_to_request.insert(device->GetId());
    if (current_device_)
      device_ids_known_to_request.insert(current_device_->GetId());
    for (const auto* device : devices_)
      device_ids_known_to_request.insert(device->GetId());

    std::set<std::string> device_ids_from_newly_started_discovery;
    for (const auto* device : discovery->GetDevices())
      device_ids_from_newly_started_discovery.insert(device->GetId());
    DCHECK(base::STLIncludes(device_ids_known_to_request,
                             device_ids_from_newly_started_discovery));
  }
#endif

  started_count_++;
  if ((state_ == State::IDLE || state_ == State::OFF) &&
      (success || started_count_ == discoveries_.size())) {
    state_ = State::IDLE;
    Transition();
  }
}

void U2fRequest::DeviceAdded(FidoDiscovery* discovery, FidoDevice* device) {
  devices_.push_back(device);

  // Start the state machine if this is the only device
  if (state_ == State::OFF) {
    state_ = State::IDLE;
    delay_callback_.Cancel();
    Transition();
  }
}

void U2fRequest::DeviceRemoved(FidoDiscovery* discovery, FidoDevice* device) {
  const std::string device_id = device->GetId();
  auto device_id_eq = [&device_id](const FidoDevice* this_device) {
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
    // After trying every device, wait 200ms before trying again.
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
