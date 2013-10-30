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

// We can't use ui/base/ime/text_input_type.h in chromeos/, so we should
// redefine that.
enum TextInputType {
  TEXT_INPUT_TYPE_NONE,
  TEXT_INPUT_TYPE_TEXT,
  TEXT_INPUT_TYPE_PASSWORD,
  TEXT_INPUT_TYPE_SEARCH,
  TEXT_INPUT_TYPE_EMAIL,
  TEXT_INPUT_TYPE_NUMBER,
  TEXT_INPUT_TYPE_TELEPHONE,
  TEXT_INPUT_TYPE_URL,
  TEXT_INPUT_TYPE_DATE,
  TEXT_INPUT_TYPE_DATE_TIME,
  TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
  TEXT_INPUT_TYPE_MONTH,
  TEXT_INPUT_TYPE_TIME,
  TEXT_INPUT_TYPE_WEEK,
  TEXT_INPUT_TYPE_TEXT_AREA,
  TEXT_INPUT_TYPE_CONTENT_EDITABLE,
  TEXT_INPUT_TYPE_DATE_TIME_FIELD,
  TEXT_INPUT_TYPE_MAX = TEXT_INPUT_TYPE_DATE_TIME_FIELD,
};

}  // namespace ibus
}  // namespace chromeos

#endif  // CHROMEOS_IME_IME_CONSTANTS_H_
