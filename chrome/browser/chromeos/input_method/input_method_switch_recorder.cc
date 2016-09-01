// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_switch_recorder.h"

#include "base/metrics/histogram_macros.h"

namespace {

chromeos::input_method::InputMethodSwitchRecorder*
    g_input_method_switch_recorder = NULL;

enum SwitchBy {
  // The values should not reordered or deleted and new entries should only be
  // added at the end (otherwise it will cause problems interpreting logs)
  SWITCH_BY_TRAY = 0,
  SWITCH_BY_ACCELERATOR = 1,
  NUM_SWITCH_BY = 2
};

}  // namespace

namespace chromeos {
namespace input_method {

// static
InputMethodSwitchRecorder* InputMethodSwitchRecorder::Get() {
  if (!g_input_method_switch_recorder)
    g_input_method_switch_recorder = new InputMethodSwitchRecorder();
  return g_input_method_switch_recorder;
}

InputMethodSwitchRecorder::InputMethodSwitchRecorder() {
}

InputMethodSwitchRecorder::~InputMethodSwitchRecorder() {
}

void InputMethodSwitchRecorder::RecordSwitch(bool by_tray_menu) {
  UMA_HISTOGRAM_ENUMERATION(
      "InputMethod.ImeSwitch",
      by_tray_menu ? SWITCH_BY_TRAY : SWITCH_BY_ACCELERATOR, NUM_SWITCH_BY);
}

}  // namespace input_method
}  // namespace chromeos

