// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <iterator>

#include "base/macros.h"
#include "base/stl_util.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

namespace {

enum SwitchProButtons {
  SWITCH_PRO_BUTTON_CAPTURE = BUTTON_INDEX_COUNT,
  SWITCH_PRO_BUTTON_COUNT
};

// The Switch Pro controller reports a larger logical range than the analog
// axes are capable of, and as a result the received axis values only use about
// 70% of the total range. We renormalize the axis values to cover the full
// range. The axis extents were determined experimentally.
const float kSwitchProAxisXMin = -0.7f;
const float kSwitchProAxisXMax = 0.7f;
const float kSwitchProAxisYMin = -0.65f;
const float kSwitchProAxisYMax = 0.75f;

// The hid-sony driver in newer kernels uses an alternate mapping for Sony
// Playstation 3 and Playstation 4 gamepads than in older kernels. To allow
// applications to distinguish between the old mapping and the new mapping,
// hid-sony sets the high bit of the device's version number.
// Dualshock 4 devices are patched in 4.10:
// https://github.com/torvalds/linux/commit/9131f8cc2b4eaf7c08d402243429e0bfba9aa0d6
// Dualshock 3 and SIXAXIS devices are patched in 4.12:
// https://github.com/torvalds/linux/commit/e19a267b9987135c00155a51e683e434b9abb56b
const uint16_t kDualshockPatchedVersion = 0x8111;

void MapperXInputStyleGamepad(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[8];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperXboxOneS(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[10];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperXboxOneS2016Firmware(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;

  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[16];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[15];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[3];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperLakeviewResearch(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no Meta on this device
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperDualshock3SixAxis(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[12]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[13]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisToButton(input.axes[8]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisToButton(input.axes[10]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = AxisToButton(input.axes[9]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[16];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperDualshock3SixAxisNew(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[16];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[10];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperDualshock4(const Gamepad& input, Gamepad* mapped) {
  enum Dualshock4Buttons {
    DUALSHOCK_BUTTON_TOUCHPAD = BUTTON_INDEX_COUNT,
    DUALSHOCK_BUTTON_COUNT
  };

  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons[DUALSHOCK_BUTTON_TOUCHPAD] = input.buttons[13];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];

  mapped->buttons_length = DUALSHOCK_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperDualshock4New(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[10];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperIBuffalo(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[1]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[1]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[0]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[0]);
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = 2;
}

void MapperXGEAR(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[2];
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no Meta on this device
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperDragonRiseGeneric(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[5]);
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no Meta on this device
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperOnLiveWireless(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[8];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperADT1(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = NullButton();
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[6];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperNvShield(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[6];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperNvShield2017(const Gamepad& input, Gamepad* mapped) {
  enum Shield2017Buttons {
    SHIELD2017_BUTTON_PLAYPAUSE = BUTTON_INDEX_COUNT,
    SHIELD2017_BUTTON_COUNT
  };
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons[SHIELD2017_BUTTON_PLAYPAUSE] = input.buttons[6];

  mapped->buttons_length = SHIELD2017_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperOUYA(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = NullButton();
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[15];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperRazerServal(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);

  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperMoga(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);

  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSamsung_EI_GP20(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = NullButton();
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = NullButton();

  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[15];

  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[3];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSteelSeriesZeemote(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = NullButton();
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = NullButton();
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_META] = NullButton();
  mapped->buttons_length = BUTTON_INDEX_META;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSteelSeriesStratusXLUsb(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[18];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[19];

  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSteelSeriesStratusXLBt(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = NullButton();
  // The BACK_SELECT and META button currently aren't mappable since they are
  // handled separately as key events, causing browser HOME and BACK actions. If
  // this is fixed, they should be added here.

  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[3];

  mapped->buttons_length = BUTTON_INDEX_META;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSwitchProUsb(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = RenormalizeAndClampAxis(
      input.axes[0], kSwitchProAxisXMin, kSwitchProAxisXMax);
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = RenormalizeAndClampAxis(
      input.axes[1], kSwitchProAxisYMin, kSwitchProAxisYMax);
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = RenormalizeAndClampAxis(
      input.axes[2], kSwitchProAxisXMin, kSwitchProAxisXMax);
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = RenormalizeAndClampAxis(
      input.axes[3], kSwitchProAxisYMin, kSwitchProAxisYMax);

  mapped->buttons_length = SWITCH_PRO_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSwitchProBluetooth(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons[SWITCH_PRO_BUTTON_CAPTURE] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = RenormalizeAndClampAxis(
      input.axes[0], kSwitchProAxisXMin, kSwitchProAxisXMax);
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = RenormalizeAndClampAxis(
      input.axes[1], kSwitchProAxisYMin, kSwitchProAxisYMax);
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = RenormalizeAndClampAxis(
      input.axes[2], kSwitchProAxisXMin, kSwitchProAxisXMax);
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = RenormalizeAndClampAxis(
      input.axes[3], kSwitchProAxisYMin, kSwitchProAxisYMax);

  mapped->buttons_length = SWITCH_PRO_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperLogitechDInput(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);

  // The Logitech button (BUTTON_INDEX_META) is not accessible through the
  // device's D-mode.
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperAnalogGamepad(const Gamepad& input, Gamepad* mapped) {
  enum AnalogGamepadButtons {
    ANALOG_GAMEPAD_BUTTON_EXTRA = BUTTON_INDEX_COUNT,
    ANALOG_GAMEPAD_BUTTON_EXTRA2,
    ANALOG_GAMEPAD_BUTTON_COUNT
  };
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[7];
  mapped->buttons[ANALOG_GAMEPAD_BUTTON_EXTRA] = input.buttons[11];
  mapped->buttons[ANALOG_GAMEPAD_BUTTON_EXTRA2] = input.buttons[12];
  mapped->buttons_length = ANALOG_GAMEPAD_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperXSkills(const Gamepad& input, Gamepad* mapped) {
  enum GamecubeButtons {
    GAMECUBE_BUTTON_LEFT_TRIGGER_CLICK = BUTTON_INDEX_COUNT,
    GAMECUBE_BUTTON_RIGHT_TRIGGER_CLICK,
    GAMECUBE_BUTTON_COUNT
  };
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];     // A
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[2];   // X
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[1];    // B
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[3];  // Y
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[6];  // Z
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = NullButton();
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_META] = NullButton();
  mapped->buttons[GAMECUBE_BUTTON_LEFT_TRIGGER_CLICK] = input.buttons[4];
  mapped->buttons[GAMECUBE_BUTTON_RIGHT_TRIGGER_CLICK] = input.buttons[5];
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[5];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[2];
  mapped->buttons_length = GAMECUBE_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperBoomN64Psx(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  // Mapped for a PSX device with Analog mode enabled.
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_META] = NullButton();
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = -input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = -input.axes[3];
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no meta
  mapped->axes_length = AXIS_INDEX_COUNT;
}

constexpr struct MappingData {
  GamepadId gamepad_id;
  GamepadStandardMappingFunction function;
} AvailableMappings[] = {
    // DragonRise Generic USB
    {GamepadId::kDragonRiseProduct0006, MapperDragonRiseGeneric},
    // Xbox 360 Wired
    {GamepadId::kMicrosoftProduct028e, MapperXInputStyleGamepad},
    // Xbox 360 Wireless
    {GamepadId::kMicrosoftProduct028f, MapperXInputStyleGamepad},
    // Xbox 360 Wireless
    {GamepadId::kMicrosoftProduct02a1, MapperXInputStyleGamepad},
    // Xbox 360 Wireless
    {GamepadId::kMicrosoftProduct0291, MapperXInputStyleGamepad},
    // Xbox One Wired
    {GamepadId::kMicrosoftProduct02d1, MapperXInputStyleGamepad},
    // Xbox One Wired (2015 FW)
    {GamepadId::kMicrosoftProduct02dd, MapperXInputStyleGamepad},
    // Xbox One S (Bluetooth)
    {GamepadId::kMicrosoftProduct02e0, MapperXboxOneS},
    // Xbox One Elite Wired
    {GamepadId::kMicrosoftProduct02e3, MapperXInputStyleGamepad},
    // Xbox One S (USB)
    {GamepadId::kMicrosoftProduct02ea, MapperXInputStyleGamepad},
    // Xbox One S (Bluetooth)
    {GamepadId::kMicrosoftProduct02fd, MapperXboxOneS2016Firmware},
    // Xbox 360 Wireless
    {GamepadId::kMicrosoftProduct0719, MapperXInputStyleGamepad},
    // Logitech F310 D-mode
    {GamepadId::kLogitechProductc216, MapperLogitechDInput},
    // Logitech F510 D-mode
    {GamepadId::kLogitechProductc218, MapperLogitechDInput},
    // Logitech F710 D-mode
    {GamepadId::kLogitechProductc219, MapperLogitechDInput},
    // Logitech F310 X-mode
    {GamepadId::kLogitechProductc21d, MapperXInputStyleGamepad},
    // Logitech F510 X-mode
    {GamepadId::kLogitechProductc21e, MapperXInputStyleGamepad},
    // Logitech F710 X-mode
    {GamepadId::kLogitechProductc21f, MapperXInputStyleGamepad},
    // Samsung Gamepad EI-GP20
    {GamepadId::kSamsungElectronicsProducta000, MapperSamsung_EI_GP20},
    // Dualshock 3 / SIXAXIS
    {GamepadId::kSonyProduct0268, MapperDualshock3SixAxis},
    // Playstation Dualshock 4
    {GamepadId::kSonyProduct05c4, MapperDualshock4},
    // Dualshock 4 (PS4 Slim)
    {GamepadId::kSonyProduct09cc, MapperDualshock4},
    // Dualshock 4 USB receiver
    {GamepadId::kSonyProduct0ba0, MapperDualshock4},
    // Switch Pro Controller
    {GamepadId::kNintendoProduct2009, MapperSwitchProUsb},
    // iBuffalo Classic
    {GamepadId::kPadixProduct2060, MapperIBuffalo},
    // SmartJoy PLUS Adapter
    {GamepadId::kLakeviewResearchProduct0005, MapperLakeviewResearch},
    // WiseGroup MP-8866
    {GamepadId::kLakeviewResearchProduct8866, MapperLakeviewResearch},
    // Nvidia Shield gamepad (2015)
    {GamepadId::kNvidiaProduct7210, MapperNvShield},
    // Nvidia Shield gamepad (2017)
    {GamepadId::kNvidiaProduct7214, MapperNvShield2017},
    // Nexus Player Controller
    {GamepadId::kAsusTekProduct4500, MapperADT1},
    // XSkills Gamecube USB adapter
    {GamepadId::kPlayComProduct0005, MapperXSkills},
    // XFXforce XGEAR PS2 Controller
    {GamepadId::kGreenAsiaProduct0003, MapperXGEAR},
    // Zeemote: SteelSeries FREE
    {GamepadId::kSteelSeriesProduct1412, MapperSteelSeriesZeemote},
    // SteelSeries Stratus XL USB
    {GamepadId::kSteelSeriesProduct1418, MapperSteelSeriesStratusXLUsb},
    // SteelSeries Stratus XL Bluetooth
    {GamepadId::kSteelSeriesBtProduct1419, MapperSteelSeriesStratusXLBt},
    // Razer Serval Controller
    {GamepadId::kRazer1532Product0900, MapperRazerServal},
    // ADT-1 Controller
    {GamepadId::kGoogleProduct2c40, MapperADT1},
    // Moga Pro Controller (HID mode)
    {GamepadId::kVendor20d6Product6271, MapperMoga},
    // Moga 2 HID
    {GamepadId::kVendor20d6Product89e5, MapperMoga},
    // OnLive Controller (Bluetooth)
    {GamepadId::kVendor2378Product1008, MapperOnLiveWireless},
    // OnLive Controller (Wired)
    {GamepadId::kVendor2378Product100a, MapperOnLiveWireless},
    // OUYA Controller
    {GamepadId::kVendor2836Product0001, MapperOUYA},
    // boom PSX+N64 USB Converter
    {GamepadId::kPrototypeVendorProduct0667, MapperBoomN64Psx},
    // Analog game controller
    {GamepadId::kPrototypeVendorProduct9401, MapperAnalogGamepad},
};

}  // namespace

GamepadStandardMappingFunction GetGamepadStandardMappingFunction(
    const uint16_t vendor_id,
    const uint16_t product_id,
    const uint16_t version_number,
    GamepadBusType bus_type) {
  GamepadId gamepad_id =
      GamepadIdList::Get().GetGamepadId(vendor_id, product_id);
  const MappingData* begin = std::begin(AvailableMappings);
  const MappingData* end = std::end(AvailableMappings);
  const auto* find_it = std::find_if(begin, end, [=](const MappingData& item) {
    return gamepad_id == item.gamepad_id;
  });
  GamepadStandardMappingFunction mapper =
      (find_it == end) ? nullptr : find_it->function;

  // The Linux kernel was updated in version 4.10 to better support Dualshock 4
  // and Dualshock 3/SIXAXIS gamepads. The driver patches the hardware version
  // when using the new mapping to allow downstream users to distinguish them.
  if (mapper == MapperDualshock4 &&
      version_number == kDualshockPatchedVersion) {
    mapper = MapperDualshock4New;
  } else if (mapper == MapperDualshock3SixAxis &&
             version_number == kDualshockPatchedVersion) {
    mapper = MapperDualshock3SixAxisNew;
  }

  // The Nintendo Switch Pro controller exposes the same product ID when
  // connected over USB or Bluetooth but communicates using different protocols.
  // In Bluetooth mode it uses standard HID, but in USB mode it uses a
  // vendor-specific protocol. Select a mapper depending on the connection type.
  if (mapper == MapperSwitchProUsb && bus_type == GAMEPAD_BUS_BLUETOOTH)
    mapper = MapperSwitchProBluetooth;

  return mapper;
}

}  // namespace device
