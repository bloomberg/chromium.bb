// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"

#include <gdk/gdk.h>

#include "third_party/WebKit/Source/WebKit/chromium/public/gtk/WebInputEventFactory.h"

using WebKit::WebInputEventFactory;

namespace {

void CopyEventTo(gfx::NativeEvent in, gfx::NativeEvent* out) {
  *out = in ? gdk_event_copy(in) : NULL;
}

void FreeEvent(gfx::NativeEvent event) {
  if (event)
    gdk_event_free(event);
}

}  // namespace

namespace content {

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : os_event(NULL),
      skip_in_browser(false),
      match_edit_command(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : WebKeyboardEvent(WebInputEventFactory::keyboardEvent(&native_event->key)),
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

}  // namespace content
