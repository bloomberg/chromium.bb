// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/public/interfaces/gamepad_struct_traits.h"

namespace mojo {

// static
void StructTraits<
    device::mojom::GamepadQuaternionDataView,
    blink::WebGamepadQuaternion>::SetToNull(blink::WebGamepadQuaternion* out) {
  memset(out, 0, sizeof(blink::WebGamepadQuaternion));
  out->notNull = false;
}

// static
bool StructTraits<device::mojom::GamepadQuaternionDataView,
                  blink::WebGamepadQuaternion>::
    Read(device::mojom::GamepadQuaternionDataView data,
         blink::WebGamepadQuaternion* out) {
  out->notNull = true;
  out->x = data.x();
  out->y = data.y();
  out->z = data.z();
  out->w = data.w();
  return true;
}

// static
void StructTraits<device::mojom::GamepadVectorDataView,
                  blink::WebGamepadVector>::SetToNull(blink::WebGamepadVector*
                                                          out) {
  memset(out, 0, sizeof(blink::WebGamepadVector));
  out->notNull = false;
}

// static
bool StructTraits<
    device::mojom::GamepadVectorDataView,
    blink::WebGamepadVector>::Read(device::mojom::GamepadVectorDataView data,
                                   blink::WebGamepadVector* out) {
  out->notNull = true;
  out->x = data.x();
  out->y = data.y();
  out->z = data.z();
  return true;
}

// static
bool StructTraits<
    device::mojom::GamepadButtonDataView,
    blink::WebGamepadButton>::Read(device::mojom::GamepadButtonDataView data,
                                   blink::WebGamepadButton* out) {
  out->pressed = data.pressed();
  out->touched = data.touched();
  out->value = data.value();
  return true;
}

// static
void StructTraits<device::mojom::GamepadPoseDataView,
                  blink::WebGamepadPose>::SetToNull(blink::WebGamepadPose*
                                                        out) {
  memset(out, 0, sizeof(blink::WebGamepadPose));
  out->notNull = false;
}

// static
bool StructTraits<device::mojom::GamepadPoseDataView, blink::WebGamepadPose>::
    Read(device::mojom::GamepadPoseDataView data, blink::WebGamepadPose* out) {
  out->notNull = true;
  if (!data.ReadOrientation(&out->orientation)) {
    return false;
  }
  out->hasOrientation = out->orientation.notNull;

  if (!data.ReadPosition(&out->position)) {
    return false;
  }
  out->hasPosition = out->position.notNull;

  if (!data.ReadAngularVelocity(&out->angularVelocity)) {
    return false;
  }
  if (!data.ReadLinearVelocity(&out->linearVelocity)) {
    return false;
  }
  if (!data.ReadAngularAcceleration(&out->angularAcceleration)) {
    return false;
  }
  if (!data.ReadLinearAcceleration(&out->linearAcceleration)) {
    return false;
  }
  return true;
}

// static
device::mojom::GamepadHand
EnumTraits<device::mojom::GamepadHand, blink::WebGamepadHand>::ToMojom(
    blink::WebGamepadHand input) {
  switch (input) {
    case blink::WebGamepadHand::GamepadHandNone:
      return device::mojom::GamepadHand::GamepadHandNone;
    case blink::WebGamepadHand::GamepadHandLeft:
      return device::mojom::GamepadHand::GamepadHandLeft;
    case blink::WebGamepadHand::GamepadHandRight:
      return device::mojom::GamepadHand::GamepadHandRight;
  }

  NOTREACHED();
  return device::mojom::GamepadHand::GamepadHandNone;
}

// static
bool EnumTraits<device::mojom::GamepadHand, blink::WebGamepadHand>::FromMojom(
    device::mojom::GamepadHand input,
    blink::WebGamepadHand* output) {
  switch (input) {
    case device::mojom::GamepadHand::GamepadHandNone:
      *output = blink::WebGamepadHand::GamepadHandNone;
      return true;
    case device::mojom::GamepadHand::GamepadHandLeft:
      *output = blink::WebGamepadHand::GamepadHandLeft;
      return true;
    case device::mojom::GamepadHand::GamepadHandRight:
      *output = blink::WebGamepadHand::GamepadHandRight;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
ConstCArray<uint16_t>
StructTraits<device::mojom::GamepadDataView, blink::WebGamepad>::id(
    const blink::WebGamepad& r) {
  size_t id_length = 0;
  while (id_length < blink::WebGamepad::idLengthCap && r.id[id_length] != 0) {
    id_length++;
  }
  return {id_length, reinterpret_cast<const uint16_t*>(&r.id[0])};
}

// static
ConstCArray<uint16_t>
StructTraits<device::mojom::GamepadDataView, blink::WebGamepad>::mapping(
    const blink::WebGamepad& r) {
  size_t mapping_length = 0;
  while (mapping_length < blink::WebGamepad::mappingLengthCap &&
         r.mapping[mapping_length] != 0) {
    mapping_length++;
  }
  return {mapping_length, reinterpret_cast<const uint16_t*>(&r.mapping[0])};
}

// static
bool StructTraits<device::mojom::GamepadDataView, blink::WebGamepad>::Read(
    device::mojom::GamepadDataView data,
    blink::WebGamepad* out) {
  out->connected = data.connected();

  memset(&out->id[0], 0,
         blink::WebGamepad::idLengthCap * sizeof(blink::WebUChar));
  CArray<uint16_t> id = {0, blink::WebGamepad::idLengthCap,
                         reinterpret_cast<uint16_t*>(&out->id[0])};
  if (!data.ReadId(&id)) {
    return false;
  }

  out->timestamp = data.timestamp();

  CArray<double> axes = {0, blink::WebGamepad::axesLengthCap, &out->axes[0]};
  if (!data.ReadAxes(&axes)) {
    return false;
  }
  // static_cast is safe when "data.ReadAxes(&axes)" above returns true.
  out->axesLength = static_cast<unsigned>(axes.size);

  CArray<blink::WebGamepadButton> buttons = {
      0, blink::WebGamepad::buttonsLengthCap, &out->buttons[0]};
  if (!data.ReadButtons(&buttons)) {
    return false;
  }
  // static_cast is safe when "data.ReadButtons(&buttons)" above returns true.
  out->buttonsLength = static_cast<unsigned>(buttons.size);

  memset(&out->mapping[0], 0,
         blink::WebGamepad::mappingLengthCap * sizeof(blink::WebUChar));
  CArray<uint16_t> mapping = {0, blink::WebGamepad::mappingLengthCap,
                              reinterpret_cast<uint16_t*>(&out->mapping[0])};
  if (!data.ReadMapping(&mapping)) {
    return false;
  }

  if (!data.ReadPose(&out->pose)) {
    return false;
  }

  blink::WebGamepadHand hand;
  if (!data.ReadHand(&hand)) {
    return false;
  }
  out->hand = hand;

  out->displayId = data.display_id();

  return true;
}

}  // namespace mojo
