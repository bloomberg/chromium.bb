// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_IME_CONSTANTS_H_
#define CHROMEOS_IME_IME_CONSTANTS_H_

namespace chromeos {

// TODO(nona): Remove ibus namespace
namespace ibus {

// Following button indicator value is introduced from
// http://developer.gnome.org/gdk/stable/gdk-Event-Structures.html#GdkEventButton
enum IBusMouseButton {
  IBUS_MOUSE_BUTTON_LEFT = 1U,
  IBUS_MOUSE_BUTTON_MIDDLE = 2U,
  IBUS_MOUSE_BUTTON_RIGHT = 3U,
};

}  // namespace ibus
}  // namespace chromeos

#endif  // CHROMEOS_IME_IME_CONSTANTS_H_
