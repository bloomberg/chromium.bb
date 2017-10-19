// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_builders_android.h"

#include <android/input.h>

#include "base/logging.h"
#include "ui/events/android/key_event_utils.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

int WebInputEventToAndroidModifier(int web_modifier) {
  int android_modifier = 0;
  // Currently only Shift, CapsLock are used, add other modifiers if required.
  if (web_modifier & WebInputEvent::kShiftKey)
    android_modifier |= AMETA_SHIFT_ON;
  if (web_modifier & WebInputEvent::kCapsLockOn)
    android_modifier |= AMETA_CAPS_LOCK_ON;
  return android_modifier;
}

ui::DomKey GetDomKeyFromEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& android_key_event,
    int keycode,
    int modifiers,
    int unicode_character) {
  // Synthetic key event, not enough information to get DomKey.
  if (android_key_event.is_null() && !unicode_character)
    return ui::DomKey::UNIDENTIFIED;

  if (!unicode_character && env) {
    // According to spec |kAllowedModifiers| should be Shift and AltGr, however
    // Android doesn't have AltGr key and ImeAdapter::getModifiers won't pass it
    // either.
    // According to discussion we want to honor CapsLock and possibly NumLock as
    // well. https://github.com/w3c/uievents/issues/70
    const int kAllowedModifiers =
        WebInputEvent::kShiftKey | WebInputEvent::kCapsLockOn;
    int fallback_modifiers =
        WebInputEventToAndroidModifier(modifiers & kAllowedModifiers);

    unicode_character = ui::events::android::GetKeyEventUnicodeChar(
        env, android_key_event, fallback_modifiers);
  }

  ui::DomKey key = ui::GetDomKeyFromAndroidEvent(keycode, unicode_character);
  if (key != ui::DomKey::NONE)
    return key;
  return ui::DomKey::UNIDENTIFIED;
}

}  // namespace

WebKeyboardEvent WebKeyboardEventBuilder::Build(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& android_key_event,
    WebInputEvent::Type type,
    int modifiers,
    double time_sec,
    int keycode,
    int scancode,
    int unicode_character,
    bool is_system_key) {
  DCHECK(WebInputEvent::IsKeyboardEventType(type));

  ui::DomCode dom_code = ui::DomCode::NONE;
  if (scancode)
    dom_code = ui::KeycodeConverter::NativeKeycodeToDomCode(scancode);

  WebKeyboardEvent result(
      type, modifiers | ui::DomCodeToWebInputEventModifiers(dom_code),
      time_sec);
  result.windows_key_code = ui::LocatedToNonLocatedKeyboardCode(
      ui::KeyboardCodeFromAndroidKeyCode(keycode));
  result.native_key_code = keycode;
  result.dom_code = static_cast<int>(dom_code);
  result.dom_key = GetDomKeyFromEvent(env, android_key_event, keycode,
                                      modifiers, unicode_character);
  result.unmodified_text[0] = unicode_character;
  if (result.windows_key_code == ui::VKEY_RETURN) {
    // This is the same behavior as GTK:
    // We need to treat the enter key as a key press of character \r. This
    // is apparently just how webkit handles it and what it expects.
    result.unmodified_text[0] = '\r';
  }
  result.text[0] = result.unmodified_text[0];
  result.is_system_key = is_system_key;

  return result;
}

WebMouseEvent WebMouseEventBuilder::Build(WebInputEvent::Type type,
                                          double time_sec,
                                          float window_x,
                                          float window_y,
                                          int modifiers,
                                          int click_count,
                                          int pointer_id,
                                          float pressure,
                                          float orientation_rad,
                                          float tilt_x,
                                          float tilt_y,
                                          int action_button,
                                          int tool_type) {
  DCHECK(WebInputEvent::IsMouseEventType(type));
  WebMouseEvent result(type, ui::EventFlagsToWebEventModifiers(modifiers),
                       time_sec);

  result.SetPositionInWidget(window_x, window_y);
  result.click_count = click_count;

  int button = action_button;
  // For events other than MouseDown/Up, action_button is not defined. So we are
  // determining |button| value from |modifiers| as is done in other platforms.
  if (type != WebInputEvent::kMouseDown && type != WebInputEvent::kMouseUp) {
    if (modifiers & ui::EF_LEFT_MOUSE_BUTTON)
      button = ui::MotionEvent::BUTTON_PRIMARY;
    else if (modifiers & ui::EF_MIDDLE_MOUSE_BUTTON)
      button = ui::MotionEvent::BUTTON_TERTIARY;
    else if (modifiers & ui::EF_RIGHT_MOUSE_BUTTON)
      button = ui::MotionEvent::BUTTON_SECONDARY;
    else
      button = 0;
  }

  ui::SetWebPointerPropertiesFromMotionEventData(result, pointer_id, pressure,
                                                 orientation_rad, tilt_x,
                                                 tilt_y, button, tool_type);

  return result;
}

WebMouseWheelEvent WebMouseWheelEventBuilder::Build(float ticks_x,
                                                    float ticks_y,
                                                    float tick_multiplier,
                                                    double time_sec,
                                                    float window_x,
                                                    float window_y) {
  WebMouseWheelEvent result(WebInputEvent::kMouseWheel,
                            WebInputEvent::kNoModifiers, time_sec);
  result.SetPositionInWidget(window_x, window_y);
  result.button = WebMouseEvent::Button::kNoButton;
  result.has_precise_scrolling_deltas = true;
  result.delta_x = ticks_x * tick_multiplier;
  result.delta_y = ticks_y * tick_multiplier;
  result.wheel_ticks_x = ticks_x;
  result.wheel_ticks_y = ticks_y;

  return result;
}

WebGestureEvent WebGestureEventBuilder::Build(WebInputEvent::Type type,
                                              double time_sec,
                                              int x,
                                              int y) {
  DCHECK(WebInputEvent::IsGestureEventType(type));
  WebGestureEvent result(type, WebInputEvent::kNoModifiers, time_sec);

  result.x = x;
  result.y = y;
  result.source_device = blink::kWebGestureDeviceTouchscreen;

  return result;
}

}  // namespace content
