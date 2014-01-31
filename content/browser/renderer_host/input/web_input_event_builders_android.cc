// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_builders_android.h"

#include "base/logging.h"
#include "content/browser/renderer_host/input/motion_event_android.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "content/browser/renderer_host/input/web_input_event_util_posix.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {
namespace {

WebInputEvent::Type ToWebInputEventType(MotionEventAndroid::Action action) {
  switch (action) {
    case MotionEventAndroid::ACTION_DOWN:
      return WebInputEvent::TouchStart;
    case MotionEventAndroid::ACTION_MOVE:
      return WebInputEvent::TouchMove;
    case MotionEventAndroid::ACTION_UP:
      return WebInputEvent::TouchEnd;
    case MotionEventAndroid::ACTION_CANCEL:
      return WebInputEvent::TouchCancel;
    case MotionEventAndroid::ACTION_POINTER_DOWN:
      return WebInputEvent::TouchStart;
    case MotionEventAndroid::ACTION_POINTER_UP:
      return WebInputEvent::TouchEnd;
  }
  NOTREACHED() << "Invalid MotionEventAndroid::Action.";
  return WebInputEvent::Undefined;
}

// Note that |is_action_pointer| is meaningful only in the context of
// |ACTION_POINTER_UP| and |ACTION_POINTER_DOWN|; other actions map directly to
// WebTouchPoint::State.
WebTouchPoint::State ToWebTouchPointState(MotionEventAndroid::Action action,
                                          bool is_action_pointer) {
  switch (action) {
    case MotionEventAndroid::ACTION_DOWN:
      return WebTouchPoint::StatePressed;
    case MotionEventAndroid::ACTION_MOVE:
      return WebTouchPoint::StateMoved;
    case MotionEventAndroid::ACTION_UP:
      return WebTouchPoint::StateReleased;
    case MotionEventAndroid::ACTION_CANCEL:
      return WebTouchPoint::StateCancelled;
    case MotionEventAndroid::ACTION_POINTER_DOWN:
      return is_action_pointer ? WebTouchPoint::StatePressed
                               : WebTouchPoint::StateStationary;
    case MotionEventAndroid::ACTION_POINTER_UP:
      return is_action_pointer ? WebTouchPoint::StateReleased
                               : WebTouchPoint::StateStationary;
  }
  NOTREACHED() << "Invalid MotionEventAndroid::Action.";
  return WebTouchPoint::StateUndefined;
}

WebTouchPoint BuildWebTouchPoint(const MotionEventAndroid& event,
                                 size_t pointer_index,
                                 float dpi_scale) {
  WebTouchPoint touch;
  touch.id = event.GetPointerId(pointer_index);
  touch.state = ToWebTouchPointState(event.GetActionMasked(),
                                     pointer_index == event.GetActionIndex());
  touch.position.x = event.GetX(pointer_index) / dpi_scale;
  touch.position.y = event.GetY(pointer_index) / dpi_scale;
  // TODO(joth): Raw event co-ordinates.
  touch.screenPosition = touch.position;

  const int radius_major =
      static_cast<int>(event.GetTouchMajor(pointer_index) * 0.5f / dpi_scale);
  const int radius_minor =
      static_cast<int>(event.GetTouchMinor(pointer_index) * 0.5f / dpi_scale);
  const float major_angle_in_radians_clockwise_from_vertical =
      event.GetOrientation();

  float major_angle_in_degrees_clockwise_from_vertical = 0;
  if (!std::isnan(major_angle_in_radians_clockwise_from_vertical)) {
    major_angle_in_degrees_clockwise_from_vertical =
        major_angle_in_radians_clockwise_from_vertical * 180.f / M_PI;
  }
  // Android provides a major axis orientation clockwise with respect to the
  // vertical of [-90, 90].  The proposed W3C extension specifies the angle that
  // the ellipse described by radiusX and radiusY is rotated clockwise about
  // its center, with a value of [0, 90], see
  // http://www.w3.org/TR/2011/WD-touch-events-20110505/.
  if (major_angle_in_degrees_clockwise_from_vertical >= 0) {
    touch.radiusX = radius_minor;
    touch.radiusY = radius_major;
    touch.rotationAngle = major_angle_in_degrees_clockwise_from_vertical;
  } else {
    touch.radiusX = radius_major;
    touch.radiusY = radius_minor;
    touch.rotationAngle = major_angle_in_degrees_clockwise_from_vertical + 90.f;
  }
  DCHECK_GE(touch.rotationAngle, 0.f);
  DCHECK_LE(touch.rotationAngle, 90.f);

  touch.force = event.GetPressure(pointer_index);

  return touch;
}

}  // namespace

WebKeyboardEvent WebKeyboardEventBuilder::Build(WebInputEvent::Type type,
                                                int modifiers,
                                                double time_sec,
                                                int keycode,
                                                int unicode_character,
                                                bool is_system_key) {
  DCHECK(WebInputEvent::isKeyboardEventType(type));
  WebKeyboardEvent result;

  result.type = type;
  result.modifiers = modifiers;
  result.timeStampSeconds = time_sec;
  ui::KeyboardCode windows_key_code =
      ui::KeyboardCodeFromAndroidKeyCode(keycode);
  UpdateWindowsKeyCodeAndKeyIdentifier(&result, windows_key_code);
  result.modifiers |= GetLocationModifiersFromWindowsKeyCode(windows_key_code);
  result.nativeKeyCode = keycode;
  result.unmodifiedText[0] = unicode_character;
  if (result.windowsKeyCode == ui::VKEY_RETURN) {
    // This is the same behavior as GTK:
    // We need to treat the enter key as a key press of character \r. This
    // is apparently just how webkit handles it and what it expects.
    result.unmodifiedText[0] = '\r';
  }
  result.text[0] = result.unmodifiedText[0];
  result.isSystemKey = is_system_key;

  return result;
}

WebMouseEvent WebMouseEventBuilder::Build(blink::WebInputEvent::Type type,
                                          WebMouseEvent::Button button,
                                          double time_sec,
                                          int window_x,
                                          int window_y,
                                          int modifiers,
                                          int click_count) {
  DCHECK(WebInputEvent::isMouseEventType(type));
  WebMouseEvent result;

  result.type = type;
  result.x = window_x;
  result.y = window_y;
  result.windowX = window_x;
  result.windowY = window_y;
  result.timeStampSeconds = time_sec;
  result.clickCount = click_count;
  result.modifiers = modifiers;

  if (type == WebInputEvent::MouseDown || type == WebInputEvent::MouseUp)
    result.button = button;
  else
    result.button = WebMouseEvent::ButtonNone;

  return result;
}

WebMouseWheelEvent WebMouseWheelEventBuilder::Build(Direction direction,
                                                    double time_sec,
                                                    int window_x,
                                                    int window_y) {
  WebMouseWheelEvent result;

  result.type = WebInputEvent::MouseWheel;
  result.x = window_x;
  result.y = window_y;
  result.windowX = window_x;
  result.windowY = window_y;
  result.timeStampSeconds = time_sec;
  result.button = WebMouseEvent::ButtonNone;

  // The below choices are matched from GTK.
  const float scrollbar_pixels_per_tick = 160.0f / 3.0f;

  switch (direction) {
    case DIRECTION_UP:
      result.deltaY = scrollbar_pixels_per_tick;
      result.wheelTicksY = 1;
      break;
    case DIRECTION_DOWN:
      result.deltaY = -scrollbar_pixels_per_tick;
      result.wheelTicksY = -1;
      break;
    case DIRECTION_LEFT:
      result.deltaX = scrollbar_pixels_per_tick;
      result.wheelTicksX = 1;
      break;
    case DIRECTION_RIGHT:
      result.deltaX = -scrollbar_pixels_per_tick;
      result.wheelTicksX = -1;
      break;
  }

  return result;
}

WebGestureEvent WebGestureEventBuilder::Build(WebInputEvent::Type type,
                                              double time_sec,
                                              int x,
                                              int y) {
  DCHECK(WebInputEvent::isGestureEventType(type));
  WebGestureEvent result;

  result.type = type;
  result.x = x;
  result.y = y;
  result.timeStampSeconds = time_sec;
  result.sourceDevice = WebGestureEvent::Touchscreen;

  return result;
}

blink::WebTouchEvent WebTouchEventBuilder::Build(jobject motion_event,
                                                 float dpi_scale) {
  DCHECK(motion_event);
  MotionEventAndroid event(motion_event);

  blink::WebTouchEvent result;

  result.type = ToWebInputEventType(event.GetActionMasked());
  DCHECK(WebInputEvent::isTouchEventType(result.type));

  result.timeStampSeconds =
      (event.GetEventTime() - base::TimeTicks()).InSecondsF();

  result.touchesLength =
      std::min(event.GetPointerCount(),
               static_cast<size_t>(WebTouchEvent::touchesLengthCap));
  DCHECK_GT(result.touchesLength, 0U);

  for (size_t i = 0; i < result.touchesLength; ++i)
    result.touches[i] = BuildWebTouchPoint(event, i, dpi_scale);

  return result;
}

}  // namespace content
