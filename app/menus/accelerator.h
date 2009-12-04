// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_MENUS_ACCELERATOR_H_
#define APP_MENUS_ACCELERATOR_H_

#include "base/keyboard_codes.h"

namespace menus {

// This is a cross-platform base class for accelerator keys used in menus. It is
// meant to be subclassed for concrete toolkit implementations.

class Accelerator {
 public:
  Accelerator() : key_code_(base::VKEY_UNKNOWN), modifiers_(0) { }
  virtual ~Accelerator() { }
  Accelerator(const Accelerator& accelerator) {
    key_code_ = accelerator.key_code_;
    modifiers_ = accelerator.modifiers_;
  }

  Accelerator& operator=(const Accelerator& accelerator) {
    if (this != &accelerator) {
      key_code_ = accelerator.key_code_;
      modifiers_ = accelerator.modifiers_;
    }
    return *this;
  }

  // We define the < operator so that the KeyboardShortcut can be used as a key
  // in a std::map.
  bool operator <(const Accelerator& rhs) const {
    if (key_code_ != rhs.key_code_)
      return key_code_ < rhs.key_code_;
    return modifiers_ < rhs.modifiers_;
  }

  bool operator ==(const Accelerator& rhs) const {
    return (key_code_ == rhs.key_code_) && (modifiers_ == rhs.modifiers_);
  }

  bool operator !=(const Accelerator& rhs) const {
    return !(*this == rhs);
  }

  base::KeyboardCode GetKeyCode() const {
    return key_code_;
  }

  int modifiers() const {
    return modifiers_;
  }

 protected:
  // The window keycode (VK_...).
  base::KeyboardCode key_code_;

  // The state of the Shift/Ctrl/Alt keys (platform-dependent).
  int modifiers_;
};

}

#endif  // APP_MENUS_ACCELERATOR_H_

