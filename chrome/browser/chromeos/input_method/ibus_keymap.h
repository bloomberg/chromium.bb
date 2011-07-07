// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_KEYMAP_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_KEYMAP_H_
#pragma once

#include <string>

namespace chromeos {
namespace input_method {

// Translate the key value from an IBus constant to a string.
std::string GetIBusKey(int keyval);

// Translate the unmodified keycode from an IBus constant to a string.
std::string GetIBusKeyCode(int keycode);

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_KEYMAP_H_
