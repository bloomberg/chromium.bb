// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/keyboard_library.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "third_party/cros/chromeos_keyboard.h"

namespace chromeos {

class KeyboardLibraryImpl : public KeyboardLibrary {
 public:
  KeyboardLibraryImpl() {}
  virtual ~KeyboardLibraryImpl() {}

  bool SetCurrentKeyboardLayoutByName(const std::string& layout_name) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      return chromeos::SetCurrentKeyboardLayoutByName(layout_name);
    }
    return false;
  }

  bool RemapModifierKeys(const ModifierMap& modifier_map) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      return chromeos::RemapModifierKeys(modifier_map);
    }
    return false;
  }

  bool SetAutoRepeatEnabled(bool enabled) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      return chromeos::SetAutoRepeatEnabled(enabled);
    }
    return false;
  }

  bool SetAutoRepeatRate(const AutoRepeatRate& rate) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      return chromeos::SetAutoRepeatRate(rate);
    }
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardLibraryImpl);
};

class KeyboardLibraryStubImpl : public KeyboardLibrary {
 public:
  KeyboardLibraryStubImpl() {}
  virtual ~KeyboardLibraryStubImpl() {}

  bool SetCurrentKeyboardLayoutByName(const std::string& layout_name) {
    return false;
  }

  bool RemapModifierKeys(const ModifierMap& modifier_map) {
    return false;
  }

  bool SetAutoRepeatEnabled(bool enabled) {
    return false;
  }

  bool SetAutoRepeatRate(const AutoRepeatRate& rate) {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardLibraryStubImpl);
};

// static
KeyboardLibrary* KeyboardLibrary::GetImpl(bool stub) {
  if (stub)
    return new KeyboardLibraryStubImpl();
  else
    return new KeyboardLibraryImpl();
}

}  // namespace chromeos
