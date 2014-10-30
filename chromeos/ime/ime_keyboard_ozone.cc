// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/fake_ime_keyboard.h"
#include "chromeos/ime/ime_keyboard.h"

namespace chromeos {
namespace input_method {

// static
ImeKeyboard* ImeKeyboard::Create() {
  return new FakeImeKeyboard;
}

}  // namespace input_method
}  // namespace chromeos
