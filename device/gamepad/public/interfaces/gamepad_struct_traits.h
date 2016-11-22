// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_PUBLIC_INTERFACES_GAMEPAD_STRUCT_TRAITS_H_
#define DEVICE_GAMEPAD_PUBLIC_INTERFACES_GAMEPAD_STRUCT_TRAITS_H_

#include <stddef.h>

#include "device/gamepad/public/interfaces/gamepad.mojom.h"
#include "mojo/public/cpp/bindings/array_traits_carray.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/WebKit/public/platform/WebGamepad.h"

namespace mojo {

template <>
struct StructTraits<device::mojom::GamepadQuaternionDataView,
                    blink::WebGamepadQuaternion> {
  static bool IsNull(const blink::WebGamepadQuaternion& r) {
    return !r.notNull;
  }
  static void SetToNull(blink::WebGamepadQuaternion* out);
  static float x(const blink::WebGamepadQuaternion& r) { return r.x; }
  static float y(const blink::WebGamepadQuaternion& r) { return r.y; }
  static float z(const blink::WebGamepadQuaternion& r) { return r.z; }
  static float w(const blink::WebGamepadQuaternion& r) { return r.w; }
  static bool Read(device::mojom::GamepadQuaternionDataView data,
                   blink::WebGamepadQuaternion* out);
};

template <>
struct StructTraits<device::mojom::GamepadVectorDataView,
                    blink::WebGamepadVector> {
  static bool IsNull(const blink::WebGamepadVector& r) { return !r.notNull; }
  static void SetToNull(blink::WebGamepadVector* out);
  static float x(const blink::WebGamepadVector& r) { return r.x; }
  static float y(const blink::WebGamepadVector& r) { return r.y; }
  static float z(const blink::WebGamepadVector& r) { return r.z; }
  static bool Read(device::mojom::GamepadVectorDataView data,
                   blink::WebGamepadVector* out);
};

template <>
struct StructTraits<device::mojom::GamepadButtonDataView,
                    blink::WebGamepadButton> {
  static bool pressed(const blink::WebGamepadButton& r) { return r.pressed; }
  static bool touched(const blink::WebGamepadButton& r) { return r.touched; }
  static double value(const blink::WebGamepadButton& r) { return r.value; }
  static bool Read(device::mojom::GamepadButtonDataView data,
                   blink::WebGamepadButton* out);
};

template <>
struct StructTraits<device::mojom::GamepadPoseDataView, blink::WebGamepadPose> {
  static bool IsNull(const blink::WebGamepadPose& r) { return !r.notNull; }
  static void SetToNull(blink::WebGamepadPose* out);
  static const blink::WebGamepadQuaternion& orientation(
      const blink::WebGamepadPose& r) {
    return r.orientation;
  }
  static const blink::WebGamepadVector& position(
      const blink::WebGamepadPose& r) {
    return r.position;
  }
  static const blink::WebGamepadVector& angular_velocity(
      const blink::WebGamepadPose& r) {
    return r.angularVelocity;
  }
  static const blink::WebGamepadVector& linear_velocity(
      const blink::WebGamepadPose& r) {
    return r.linearVelocity;
  }
  static const blink::WebGamepadVector& angular_acceleration(
      const blink::WebGamepadPose& r) {
    return r.angularAcceleration;
  }
  static const blink::WebGamepadVector& linear_acceleration(
      const blink::WebGamepadPose& r) {
    return r.linearAcceleration;
  }
  static bool Read(device::mojom::GamepadPoseDataView data,
                   blink::WebGamepadPose* out);
};

template <>
struct EnumTraits<device::mojom::GamepadHand, blink::WebGamepadHand> {
  static device::mojom::GamepadHand ToMojom(blink::WebGamepadHand input);
  static bool FromMojom(device::mojom::GamepadHand input,
                        blink::WebGamepadHand* output);
};

template <>
struct StructTraits<device::mojom::GamepadDataView, blink::WebGamepad> {
  static bool connected(const blink::WebGamepad& r) { return r.connected; }
  static uint64_t timestamp(const blink::WebGamepad& r) { return r.timestamp; }
  static ConstCArray<double> axes(const blink::WebGamepad& r) {
    return {r.axesLength, &r.axes[0]};
  }
  static ConstCArray<blink::WebGamepadButton> buttons(
      const blink::WebGamepad& r) {
    return {r.buttonsLength, &r.buttons[0]};
  }
  static const blink::WebGamepadPose& pose(const blink::WebGamepad& r) {
    return r.pose;
  }
  static const blink::WebGamepadHand& hand(const blink::WebGamepad& r) {
    return r.hand;
  }
  static uint32_t display_id(const blink::WebGamepad& r) { return r.displayId; }

  static ConstCArray<uint16_t> id(const blink::WebGamepad& r);
  static ConstCArray<uint16_t> mapping(const blink::WebGamepad& r);
  static bool Read(device::mojom::GamepadDataView data, blink::WebGamepad* out);
};

}  // namespace mojo

#endif  // DEVICE_GAMEPAD_PUBLIC_INTERFACES_GAMEPAD_STRUCT_TRAITS_H_
