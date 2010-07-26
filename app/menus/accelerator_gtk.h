// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_MENUS_ACCELERATOR_GTK_H_
#define APP_MENUS_ACCELERATOR_GTK_H_
#pragma once

#include <gdk/gdk.h>

#include "app/menus/accelerator.h"
#include "base/keyboard_code_conversion_gtk.h"
#include "base/keyboard_codes_posix.h"

namespace menus {

class AcceleratorGtk : public Accelerator {
 public:
  AcceleratorGtk(base::KeyboardCode key_code,
                 bool shift_pressed, bool ctrl_pressed, bool alt_pressed)
      : gdk_keyval_(0) {
    key_code_ = key_code;
    modifiers_ = 0;
    if (shift_pressed)
      modifiers_ |= GDK_SHIFT_MASK;
    if (ctrl_pressed)
      modifiers_ |= GDK_CONTROL_MASK;
    if (alt_pressed)
      modifiers_ |= GDK_MOD1_MASK;
  }

  AcceleratorGtk(guint keyval, GdkModifierType modifier_type) {
    key_code_ = base::WindowsKeyCodeForGdkKeyCode(keyval);
    gdk_keyval_ = keyval;
    modifiers_ = modifier_type;
  }

  AcceleratorGtk() : gdk_keyval_(0) { }
  virtual ~AcceleratorGtk() { }

  guint GetGdkKeyCode() const {
    return gdk_keyval_ > 0 ?
           // The second parameter is false because accelerator keys are
           // expressed in terms of the non-shift-modified key.
           gdk_keyval_ : base::GdkKeyCodeForWindowsKeyCode(key_code_, false);
  }

  GdkModifierType gdk_modifier_type() {
    return static_cast<GdkModifierType>(modifiers());
  }

 private:
  // The GDK keycode.
  guint gdk_keyval_;
};

}  // namespace menus

#endif  // APP_MENUS_ACCELERATOR_GTK_H_
