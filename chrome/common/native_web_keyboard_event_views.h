// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NATIVE_WEB_KEYBOARD_EVENT_VIEWS_H_
#define CHROME_COMMON_NATIVE_WEB_KEYBOARD_EVENT_VIEWS_H_
#pragma once

#include "content/common/native_web_keyboard_event.h"

namespace views {
class KeyEvent;
}

// A views implementation of NativeWebKeyboardEvent.
struct NativeWebKeyboardEventViews : public NativeWebKeyboardEvent {
  // TODO(suzhe): remove once we get rid of Gtk from Views.
  struct FromViewsEvent {};
  // These two constructors are shared between Windows and Linux Views ports.
  explicit NativeWebKeyboardEventViews(const views::KeyEvent& event);
  // TODO(suzhe): Sadly, we need to add a meanless FromViewsEvent parameter to
  // distinguish between this contructor and above Gtk one, because they use
  // different modifier flags. We can remove this extra parameter as soon as we
  // disable above Gtk constructor in Linux Views port.
  NativeWebKeyboardEventViews(uint16 character,
                              int flags,
                              double time_stamp_seconds,
                              FromViewsEvent);

  ~NativeWebKeyboardEventViews();
};

#endif  // CHROME_COMMON_NATIVE_WEB_KEYBOARD_EVENT_VIEWS_H_
