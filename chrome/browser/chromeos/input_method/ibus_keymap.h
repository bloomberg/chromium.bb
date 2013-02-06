// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_KEYMAP_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_KEYMAP_H_

#include <string>
#include "base/basictypes.h"

namespace chromeos {
namespace input_method {

// Translates the key value from an IBus constant to a string.
std::string GetIBusKey(int keyval);

// Translates the unmodified keycode from an IBus constant to a string.
std::string GetIBusKeyCode(uint16 keycode);

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_KEYMAP_H_
