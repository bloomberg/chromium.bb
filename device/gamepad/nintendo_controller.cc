// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/nintendo_controller.h"

#include "device/gamepad/gamepad_data_fetcher.h"

namespace device {
namespace {
// Device IDs for known Switch devices.
const uint16_t kVendorNintendo = 0x057e;
const uint16_t kProductSwitchProController = 0x2009;

// Device name for a composite Joy-Con device.
const char kProductNameSwitchCompositeDevice[] = "Joy-Con L+R";
}  // namespace

NintendoController::NintendoController(int source_id,
                                       mojom::HidDeviceInfoPtr device_info,
                                       mojom::HidManager* hid_manager)
    : source_id_(source_id),
      is_composite_(false),
      device_info_(std::move(device_info)),
      hid_manager_(hid_manager),
      weak_factory_(this) {}

NintendoController::NintendoController(
    int source_id,
    std::unique_ptr<NintendoController> composite1,
    std::unique_ptr<NintendoController> composite2,
    mojom::HidManager* hid_manager)
    : source_id_(source_id),
      is_composite_(true),
      hid_manager_(hid_manager),
      weak_factory_(this) {
  // Require exactly one left component and one right component, but allow them
  // to be provided in either order.
  DCHECK(composite1);
  DCHECK(composite2);
  composite_left_ = std::move(composite1);
  composite_right_ = std::move(composite2);
  if (composite_left_->GetGamepadHand() != GamepadHand::kLeft)
    composite_left_.swap(composite_right_);
  DCHECK_EQ(composite_left_->GetGamepadHand(), GamepadHand::kLeft);
  DCHECK_EQ(composite_right_->GetGamepadHand(), GamepadHand::kRight);
}

NintendoController::~NintendoController() = default;

// static
std::unique_ptr<NintendoController> NintendoController::Create(
    int source_id,
    mojom::HidDeviceInfoPtr device_info,
    mojom::HidManager* hid_manager) {
  return std::make_unique<NintendoController>(source_id, std::move(device_info),
                                              hid_manager);
}

// static
std::unique_ptr<NintendoController> NintendoController::CreateComposite(
    int source_id,
    std::unique_ptr<NintendoController> composite1,
    std::unique_ptr<NintendoController> composite2,
    mojom::HidManager* hid_manager) {
  return std::make_unique<NintendoController>(
      source_id, std::move(composite1), std::move(composite2), hid_manager);
}

// static
bool NintendoController::IsNintendoController(uint16_t vendor_id,
                                              uint16_t product_id) {
  // TODO(mattreynolds): Recognize Nintendo Switch devices.
  return false;
}

std::vector<std::unique_ptr<NintendoController>>
NintendoController::Decompose() {
  std::vector<std::unique_ptr<NintendoController>> decomposed_devices;
  if (composite_left_)
    decomposed_devices.push_back(std::move(composite_left_));
  if (composite_right_)
    decomposed_devices.push_back(std::move(composite_right_));
  return decomposed_devices;
}

void NintendoController::Open(base::OnceClosure device_ready_closure) {
  device_ready_closure_ = std::move(device_ready_closure);
  if (is_composite_) {
    StartInitSequence();
  } else {
    uint16_t vendor_id = device_info_->vendor_id;
    uint16_t product_id = device_info_->product_id;
    if (IsNintendoController(vendor_id, product_id)) {
      Connect(base::BindOnce(&NintendoController::OnConnect,
                             weak_factory_.GetWeakPtr()));
    }
  }
}

GamepadHand NintendoController::GetGamepadHand() const {
  // TODO(mattreynolds): Determine device handedness.
  return GamepadHand::kNone;
}

bool NintendoController::IsUsable() const {
  // TODO(mattreynolds): Determine if a device is usable.
  NOTIMPLEMENTED();
  return false;
}

bool NintendoController::HasGuid(const std::string& guid) const {
  if (is_composite_)
    return composite_left_->HasGuid(guid) || composite_right_->HasGuid(guid);
  else
    return device_info_->guid == guid;
}

GamepadStandardMappingFunction NintendoController::GetMappingFunction() const {
  if (is_composite_) {
    // In composite mode, we use the same mapping as a Pro Controller.
    return GetGamepadStandardMappingFunction(
        kVendorNintendo, kProductSwitchProController, 0, bus_type_);
  } else {
    // Version number is not available through the HID service.
    return GetGamepadStandardMappingFunction(
        device_info_->vendor_id, device_info_->product_id, 0, bus_type_);
  }
}

void NintendoController::InitializeGamepadState(bool has_standard_mapping,
                                                Gamepad& pad) const {
  pad.buttons_length = device::BUTTON_INDEX_COUNT + 1;
  pad.axes_length = device::AXIS_INDEX_COUNT;
  pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
  pad.vibration_actuator.not_null = true;
  pad.timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
  if (is_composite_) {
    // Composite devices use the same product ID as the Switch Pro Controller,
    // and expose the same buttons and axes.
    GamepadDataFetcher::UpdateGamepadStrings(
        kProductNameSwitchCompositeDevice, kVendorNintendo,
        kProductSwitchProController, has_standard_mapping, pad);
  } else {
    GamepadDataFetcher::UpdateGamepadStrings(
        device_info_->product_name, device_info_->vendor_id,
        device_info_->product_id, has_standard_mapping, pad);
  }
}

void NintendoController::UpdateGamepadState(Gamepad& pad) const {
  // TODO(mattreynolds): Read gamepad state.
  NOTIMPLEMENTED();
}

void NintendoController::Connect(mojom::HidManager::ConnectCallback callback) {
  DCHECK(!is_composite_);
  DCHECK(hid_manager_);
  hid_manager_->Connect(device_info_->guid, std::move(callback));
}

void NintendoController::OnConnect(mojom::HidConnectionPtr connection) {
  if (connection) {
    connection_ = std::move(connection);
    // TODO(mattreynolds): Start listening for input reports.
    StartInitSequence();
  }
}

void NintendoController::StartInitSequence() {
  // TODO(mattreynolds): Implement initialization sequence.
  NOTIMPLEMENTED();
}

void NintendoController::DoShutdown() {
  if (composite_left_)
    composite_left_->Shutdown();
  composite_left_.reset();
  if (composite_right_)
    composite_right_->Shutdown();
  composite_right_.reset();
  connection_.reset();
  device_info_.reset();
}

void NintendoController::SetVibration(double strong_magnitude,
                                      double weak_magnitude) {
  // TODO(mattreynolds): Set vibration.
  NOTIMPLEMENTED();
}

}  // namespace device
