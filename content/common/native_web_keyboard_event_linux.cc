// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/native_web_keyboard_event.h"

#include <gdk/gdk.h>

#include "third_party/WebKit/Source/WebKit/chromium/public/gtk/WebInputEventFactory.h"

using WebKit::WebInputEventFactory;

namespace {

void CopyEventTo(const GdkEventKey* in, GdkEventKey** out) {
  if (in) {
    *out = reinterpret_cast<GdkEventKey*>(
        gdk_event_copy(
            reinterpret_cast<GdkEvent*>(const_cast<GdkEventKey*>(in))));
  } else {
    *out = NULL;
  }
}

void FreeEvent(GdkEventKey* event) {
  if (event) {
    gdk_event_free(reinterpret_cast<GdkEvent*>(event));
  }
}

}  // namespace


NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : os_event(NULL),
      skip_in_browser(false),
      match_edit_command(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(const GdkEventKey* native_event)
    : WebKeyboardEvent(WebInputEventFactory::keyboardEvent(native_event)),
      skip_in_browser(false),
      match_edit_command(false) {
  CopyEventTo(native_event, &os_event);
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(wchar_t character,
                                               int state,
                                               double time_stamp_seconds)
    : WebKeyboardEvent(WebInputEventFactory::keyboardEvent(character,
                                                           state,
                                                           time_stamp_seconds)),
      os_event(NULL),
      skip_in_browser(false),
      match_edit_command(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      skip_in_browser(other.skip_in_browser),
      match_edit_command(other.match_edit_command) {
  CopyEventTo(other.os_event, &os_event);
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);

  FreeEvent(os_event);
  CopyEventTo(other.os_event, &os_event);

  skip_in_browser = other.skip_in_browser;
  match_edit_command = other.match_edit_command;

  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  FreeEvent(os_event);
}
