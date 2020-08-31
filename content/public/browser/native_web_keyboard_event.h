// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_WEB_KEYBOARD_EVENT_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_WEB_KEYBOARD_EVENT_H_

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace ui {
class KeyEvent;
}

namespace content {

// Owns a platform specific event; used to pass own and pass event through
// platform independent code.
struct CONTENT_EXPORT NativeWebKeyboardEvent : public blink::WebKeyboardEvent {
  NativeWebKeyboardEvent(blink::WebInputEvent::Type type,
                         int modifiers,
                         base::TimeTicks timestamp);

  // Creates a native web keyboard event from a WebKeyboardEvent. The |os_event|
  // member may be a synthetic event, and possibly incomplete.
  NativeWebKeyboardEvent(const blink::WebKeyboardEvent& web_event,
                         gfx::NativeView native_view);

  explicit NativeWebKeyboardEvent(gfx::NativeEvent native_event);
#if defined(OS_ANDROID)
  // Holds a global ref to android_key_event (allowed to be null).
  NativeWebKeyboardEvent(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& android_key_event,
      blink::WebInputEvent::Type type,
      int modifiers,
      base::TimeTicks timestamp,
      int keycode,
      int scancode,
      int unicode_character,
      bool is_system_key);
#else
  explicit NativeWebKeyboardEvent(const ui::KeyEvent& key_event);
#if defined(USE_AURA)
  // Create a legacy keypress event specified by |character|.
  NativeWebKeyboardEvent(const ui::KeyEvent& key_event, base::char16 character);
#endif
#endif

#if defined(OS_MACOSX)
  // TODO(bokan): Temporarily added to debug https://crbug.com/1039833. This is
  // used to allow collecting Event.Latency.OS_NO_VALIDATION only in contexts
  // where the key event will be sent to the renderer.  The purpose is to avoid
  // recording it for reinjected events after the renderer has already
  // processed the event.
  static NativeWebKeyboardEvent CreateForRenderer(
      gfx::NativeEvent native_event);
  NativeWebKeyboardEvent(gfx::NativeEvent native_event, bool record_debug_uma);
#endif

  NativeWebKeyboardEvent(const NativeWebKeyboardEvent& event);
  ~NativeWebKeyboardEvent() override;

  NativeWebKeyboardEvent& operator=(const NativeWebKeyboardEvent& event);

  gfx::NativeEvent os_event;

  // True if the browser should ignore this event if it's not handled by the
  // renderer. This happens for RawKeyDown events that are created while IME is
  // active and is necessary to prevent backspace from doing "history back" if
  // it is hit in ime mode.
  // Currently, it's only used by Linux and Mac ports.
  bool skip_in_browser;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_WEB_KEYBOARD_EVENT_H_
