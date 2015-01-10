// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_SWITCH_RECORDER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_SWITCH_RECORDER_H_

namespace chromeos {
namespace input_method {

// A class for recording UMA logs for IME switches.
class InputMethodSwitchRecorder {
 public:
  InputMethodSwitchRecorder();
  ~InputMethodSwitchRecorder();

  static InputMethodSwitchRecorder* Get();
  void RecordSwitch(bool by_tray_menu);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_SWITCH_RECORDER_H_

