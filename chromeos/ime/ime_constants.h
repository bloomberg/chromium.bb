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

// Following variables indicate state of IBusProperty.
enum IBusPropertyState {
  IBUS_PROPERTY_STATE_UNCHECKED = 0,
  IBUS_PROPERTY_STATE_CHECKED = 1,
  IBUS_PROPERTY_STATE_INCONSISTENT = 2,
};

// We can't use ui/gfx/rect.h in chromeos/, so we should use ibus::Rect instead.
struct Rect {
 Rect() : x(0), y(0), width(0), height(0) {}
 Rect(int x, int y, int width, int height)
     : x(x),
       y(y),
       width(width),
       height(height) {}
 int x;
 int y;
 int width;
 int height;
};

}  // namespace ibus
}  // namespace chromeos

#endif  // CHROMEOS_IME_IME_CONSTANTS_H_
