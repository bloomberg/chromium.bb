// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_gamepad_data_fetcher.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"

#include "device/vr/android/gvr/gvr_gamepad_data_provider.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace device {

namespace {

void CopyToWebUString(blink::WebUChar* dest,
                      size_t dest_length,
                      base::string16 src) {
  static_assert(sizeof(base::string16::value_type) == sizeof(blink::WebUChar),
                "Mismatched string16/WebUChar size.");

  const size_t str_to_copy = std::min(src.size(), dest_length - 1);
  memcpy(dest, src.data(), str_to_copy * sizeof(base::string16::value_type));
  dest[str_to_copy] = 0;
}

}  // namespace

using namespace blink;

GvrGamepadDataFetcher::Factory::Factory(GvrGamepadDataProvider* data_provider,
                                        unsigned int display_id)
    : data_provider_(data_provider), display_id_(display_id) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
}

GvrGamepadDataFetcher::Factory::~Factory() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
}

std::unique_ptr<GamepadDataFetcher>
GvrGamepadDataFetcher::Factory::CreateDataFetcher() {
  return base::MakeUnique<GvrGamepadDataFetcher>(data_provider_, display_id_);
}

GamepadSource GvrGamepadDataFetcher::Factory::source() {
  return GAMEPAD_SOURCE_GVR;
}

GvrGamepadDataFetcher::GvrGamepadDataFetcher(
    GvrGamepadDataProvider* data_provider,
    unsigned int display_id)
    : display_id_(display_id) {
  // Called on UI thread.
  DVLOG(1) << __FUNCTION__ << "=" << this;
  data_provider->RegisterGamepadDataFetcher(this);
}

GvrGamepadDataFetcher::~GvrGamepadDataFetcher() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
}

GamepadSource GvrGamepadDataFetcher::source() {
  return GAMEPAD_SOURCE_GVR;
}

void GvrGamepadDataFetcher::OnAddedToProvider() {
  PauseHint(false);
}

void GvrGamepadDataFetcher::SetGamepadData(GvrGamepadData data) {
  // Called from UI thread.
  gamepad_data_ = data;
}

void GvrGamepadDataFetcher::GetGamepadData(bool devices_changed_hint) {
  // Called from gamepad polling thread.

  PadState* state = GetPadState(0);
  if (!state)
    return;

  // Take a snapshot of the asynchronously updated gamepad data.
  // TODO(bajones): ensure consistency?
  GvrGamepadData provided_data = gamepad_data_;

  WebGamepad& pad = state->data;
  if (state->active_state == GAMEPAD_NEWLY_ACTIVE) {
    // This is the first time we've seen this device, so do some one-time
    // initialization
    pad.connected = true;
    CopyToWebUString(pad.id, WebGamepad::idLengthCap,
                     base::UTF8ToUTF16("Daydream Controller"));
    CopyToWebUString(pad.mapping, WebGamepad::mappingLengthCap,
                     base::UTF8ToUTF16(""));
    pad.buttonsLength = 1;
    pad.axesLength = 2;

    pad.displayId = display_id_;

    pad.hand = provided_data.right_handed ? GamepadHandRight : GamepadHandLeft;
  }

  pad.timestamp = provided_data.timestamp;

  if (provided_data.is_touching) {
    gvr_vec2f touch_position = provided_data.touch_pos;
    pad.axes[0] = (touch_position.x * 2.0f) - 1.0f;
    pad.axes[1] = (touch_position.y * 2.0f) - 1.0f;
  } else {
    pad.axes[0] = 0.0f;
    pad.axes[1] = 0.0f;
  }

  pad.buttons[0].touched = provided_data.is_touching;
  pad.buttons[0].pressed = provided_data.controller_button_pressed;
  pad.buttons[0].value = pad.buttons[0].pressed ? 1.0f : 0.0f;

  pad.pose.notNull = true;
  pad.pose.hasOrientation = true;
  pad.pose.hasPosition = false;

  gvr_quatf orientation = provided_data.orientation;
  pad.pose.orientation.notNull = true;
  pad.pose.orientation.x = orientation.qx;
  pad.pose.orientation.y = orientation.qy;
  pad.pose.orientation.z = orientation.qz;
  pad.pose.orientation.w = orientation.qw;

  gvr_vec3f accel = provided_data.accel;
  pad.pose.linearAcceleration.notNull = true;
  pad.pose.linearAcceleration.x = accel.x;
  pad.pose.linearAcceleration.y = accel.y;
  pad.pose.linearAcceleration.z = accel.z;

  gvr_vec3f gyro = provided_data.gyro;
  pad.pose.angularVelocity.notNull = true;
  pad.pose.angularVelocity.x = gyro.x;
  pad.pose.angularVelocity.y = gyro.y;
  pad.pose.angularVelocity.z = gyro.z;
}

void GvrGamepadDataFetcher::PauseHint(bool paused) {}

}  // namespace device
