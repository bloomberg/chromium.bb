// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/input_handler.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/compositor_frame_metadata.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"

namespace content {
namespace protocol {

namespace {

gfx::PointF CssPixelsToPointF(int x, int y, float page_scale_factor) {
  return gfx::PointF(x * page_scale_factor, y * page_scale_factor);
}

gfx::Vector2dF CssPixelsToVector2dF(int x, int y, float page_scale_factor) {
  return gfx::Vector2dF(x * page_scale_factor, y * page_scale_factor);
}

bool StringToGestureSourceType(Maybe<std::string> in,
                               SyntheticGestureParams::GestureSourceType& out) {
  if (!in.isJust()) {
    out = SyntheticGestureParams::GestureSourceType::DEFAULT_INPUT;
    return true;
  }
  if (in.fromJust() == Input::GestureSourceTypeEnum::Default) {
    out = SyntheticGestureParams::GestureSourceType::DEFAULT_INPUT;
    return true;
  }
  if (in.fromJust() == Input::GestureSourceTypeEnum::Touch) {
    out = SyntheticGestureParams::GestureSourceType::TOUCH_INPUT;
    return true;
  }
  if (in.fromJust() == Input::GestureSourceTypeEnum::Mouse) {
    out = SyntheticGestureParams::GestureSourceType::MOUSE_INPUT;
    return true;
  }
  return false;
}

void SetEventModifiers(blink::WebInputEvent* event, int modifiers) {
  if (modifiers & 1)
    event->modifiers |= blink::WebInputEvent::AltKey;
  if (modifiers & 2)
    event->modifiers |= blink::WebInputEvent::ControlKey;
  if (modifiers & 4)
    event->modifiers |= blink::WebInputEvent::MetaKey;
  if (modifiers & 8)
    event->modifiers |= blink::WebInputEvent::ShiftKey;
}

void SetEventTimestamp(blink::WebInputEvent* event, Maybe<double> timestamp) {
  // Convert timestamp, in seconds since unix epoch, to an event timestamp
  // which is time ticks since platform start time.
  base::TimeTicks ticks = timestamp.isJust()
      ? base::TimeDelta::FromSecondsD(timestamp.fromJust()) +
          base::TimeTicks::UnixEpoch()
      : base::TimeTicks::Now();
  event->timeStampSeconds = (ticks - base::TimeTicks()).InSecondsF();
}

bool SetKeyboardEventText(blink::WebUChar* to, Maybe<std::string> from) {
  if (!from.isJust())
    return true;

  base::string16 text16 = base::UTF8ToUTF16(from.fromJust());
  if (text16.size() > blink::WebKeyboardEvent::textLengthCap)
    return false;

  for (size_t i = 0; i < text16.size(); ++i)
    to[i] = text16[i];
  return true;
}

bool SetMouseEventButton(blink::WebMouseEvent* event,
                         const std::string& button) {
  if (button.empty())
    return true;

  if (button == Input::DispatchMouseEvent::ButtonEnum::None) {
    event->button = blink::WebMouseEvent::Button::NoButton;
  } else if (button == Input::DispatchMouseEvent::ButtonEnum::Left) {
    event->button = blink::WebMouseEvent::Button::Left;
    event->modifiers |= blink::WebInputEvent::LeftButtonDown;
  } else if (button == Input::DispatchMouseEvent::ButtonEnum::Middle) {
    event->button = blink::WebMouseEvent::Button::Middle;
    event->modifiers |= blink::WebInputEvent::MiddleButtonDown;
  } else if (button == Input::DispatchMouseEvent::ButtonEnum::Right) {
    event->button = blink::WebMouseEvent::Button::Right;
    event->modifiers |= blink::WebInputEvent::RightButtonDown;
  } else {
    return false;
  }
  return true;
}

bool SetMouseEventType(blink::WebMouseEvent* event, const std::string& type) {
  if (type == Input::DispatchMouseEvent::TypeEnum::MousePressed) {
    event->type = blink::WebInputEvent::MouseDown;
  } else if (type == Input::DispatchMouseEvent::TypeEnum::MouseReleased) {
    event->type = blink::WebInputEvent::MouseUp;
  } else if (type == Input::DispatchMouseEvent::TypeEnum::MouseMoved) {
    event->type = blink::WebInputEvent::MouseMove;
  } else {
    return false;
  }
  return true;
}

void SendSynthesizePinchGestureResponse(
    std::unique_ptr<Input::Backend::SynthesizePinchGestureCallback> callback,
    SyntheticGesture::Result result) {
  if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
    callback->sendSuccess();
  } else {
    callback->sendFailure(Response::Error(
        base::StringPrintf("Synthetic pinch failed, result was %d", result)));
  }
}

class TapGestureResponse {
 public:
  TapGestureResponse(
      std::unique_ptr<Input::Backend::SynthesizeTapGestureCallback> callback,
      int count)
      : callback_(std::move(callback)),
        count_(count) {
  }

  void OnGestureResult(SyntheticGesture::Result result) {
    --count_;
    // Still waiting for more taps to finish.
    if (result == SyntheticGesture::Result::GESTURE_FINISHED && count_)
      return;
    if (callback_) {
      if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
        callback_->sendSuccess();
      } else {
        callback_->sendFailure(Response::Error(
            base::StringPrintf("Synthetic tap failed, result was %d", result)));
      }
      callback_.reset();
    }
    if (!count_)
      delete this;
  }

 private:
  std::unique_ptr<Input::Backend::SynthesizeTapGestureCallback> callback_;
  int count_;
};

void SendSynthesizeScrollGestureResponse(
    std::unique_ptr<Input::Backend::SynthesizeScrollGestureCallback> callback,
    SyntheticGesture::Result result) {
  if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
    callback->sendSuccess();
  } else {
    callback->sendFailure(Response::Error(
        base::StringPrintf("Synthetic scroll failed, result was %d", result)));
  }
}

}  // namespace

InputHandler::InputHandler()
    : host_(NULL),
      page_scale_factor_(1.0),
      last_id_(0),
      weak_factory_(this) {
}

InputHandler::~InputHandler() {
}

void InputHandler::SetRenderFrameHost(RenderFrameHostImpl* host) {
  host_ = host;
}

void InputHandler::Wire(UberDispatcher* dispatcher) {
  Input::Dispatcher::wire(dispatcher, this);
}

void InputHandler::OnSwapCompositorFrame(
    const cc::CompositorFrameMetadata& frame_metadata) {
  page_scale_factor_ = frame_metadata.page_scale_factor;
  scrollable_viewport_size_ = frame_metadata.scrollable_viewport_size;
}

Response InputHandler::Disable() {
  return Response::OK();
}

Response InputHandler::DispatchKeyEvent(
    const std::string& type,
    Maybe<int> modifiers,
    Maybe<double> timestamp,
    Maybe<std::string> text,
    Maybe<std::string> unmodified_text,
    Maybe<std::string> key_identifier,
    Maybe<std::string> code,
    Maybe<std::string> key,
    Maybe<int> windows_virtual_key_code,
    Maybe<int> native_virtual_key_code,
    Maybe<bool> auto_repeat,
    Maybe<bool> is_keypad,
    Maybe<bool> is_system_key) {
  NativeWebKeyboardEvent event;
  event.skip_in_browser = true;

  if (type == Input::DispatchKeyEvent::TypeEnum::KeyDown) {
    event.type = blink::WebInputEvent::KeyDown;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::KeyUp) {
    event.type = blink::WebInputEvent::KeyUp;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::Char) {
    event.type = blink::WebInputEvent::Char;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::RawKeyDown) {
    event.type = blink::WebInputEvent::RawKeyDown;
  } else {
    return Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str()));
  }

  SetEventModifiers(&event, modifiers.fromMaybe(0));
  SetEventTimestamp(&event, std::move(timestamp));
  if (!SetKeyboardEventText(event.text, std::move(text)))
    return Response::InvalidParams("Invalid 'text' parameter");
  if (!SetKeyboardEventText(event.unmodifiedText, std::move(unmodified_text)))
    return Response::InvalidParams("Invalid 'unmodifiedText' parameter");

  if (windows_virtual_key_code.isJust())
    event.windowsKeyCode = windows_virtual_key_code.fromJust();
  if (native_virtual_key_code.isJust())
    event.nativeKeyCode = native_virtual_key_code.fromJust();
  if (auto_repeat.fromMaybe(false))
    event.modifiers |= blink::WebInputEvent::IsAutoRepeat;
  if (is_keypad.fromMaybe(false))
    event.modifiers |= blink::WebInputEvent::IsKeyPad;
  if (is_system_key.isJust())
    event.isSystemKey = is_system_key.fromJust();

  if (code.isJust()) {
    event.domCode = static_cast<int>(
        ui::KeycodeConverter::CodeStringToDomCode(code.fromJust()));
  }

  if (key.isJust()) {
    event.domKey = static_cast<int>(
        ui::KeycodeConverter::KeyStringToDomKey(key.fromJust()));
  }

  if (!host_ || !host_->GetRenderWidgetHost())
    return Response::InternalError();

  host_->GetRenderWidgetHost()->Focus();
  host_->GetRenderWidgetHost()->ForwardKeyboardEvent(event);
  return Response::OK();
}

Response InputHandler::DispatchMouseEvent(
    const std::string& type,
    int x,
    int y,
    Maybe<int> modifiers,
    Maybe<double> timestamp,
    Maybe<std::string> button,
    Maybe<int> click_count) {
  blink::WebMouseEvent event;

  if (!SetMouseEventType(&event, type)) {
    return Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str()));
  }
  SetEventModifiers(&event, modifiers.fromMaybe(0));
  SetEventTimestamp(&event, std::move(timestamp));
  if (!SetMouseEventButton(&event, button.fromMaybe("")))
    return Response::InvalidParams("Invalid mouse button");

  event.x = x * page_scale_factor_;
  event.y = y * page_scale_factor_;
  event.windowX = x * page_scale_factor_;
  event.windowY = y * page_scale_factor_;
  event.globalX = x * page_scale_factor_;
  event.globalY = y * page_scale_factor_;
  event.clickCount = click_count.fromMaybe(0);
  event.pointerType = blink::WebPointerProperties::PointerType::Mouse;

  if (!host_ || !host_->GetRenderWidgetHost())
    return Response::InternalError();

  host_->GetRenderWidgetHost()->Focus();
  host_->GetRenderWidgetHost()->ForwardMouseEvent(event);
  return Response::OK();
}

Response InputHandler::EmulateTouchFromMouseEvent(const std::string& type,
                                                  int x,
                                                  int y,
                                                  double timestamp,
                                                  const std::string& button,
                                                  Maybe<double> delta_x,
                                                  Maybe<double> delta_y,
                                                  Maybe<int> modifiers,
                                                  Maybe<int> click_count) {
  blink::WebMouseWheelEvent wheel_event;
  blink::WebMouseEvent mouse_event;
  blink::WebMouseEvent* event = &mouse_event;

  if (type == Input::EmulateTouchFromMouseEvent::TypeEnum::MouseWheel) {
    if (!delta_x.isJust() || !delta_y.isJust()) {
      return Response::InvalidParams(
          "'deltaX' and 'deltaY' are expected for mouseWheel event");
    }
    wheel_event.deltaX = static_cast<float>(delta_x.fromJust());
    wheel_event.deltaY = static_cast<float>(delta_y.fromJust());
    event = &wheel_event;
    event->type = blink::WebInputEvent::MouseWheel;
  } else if (!SetMouseEventType(event, type)) {
    return Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str()));
  }

  SetEventModifiers(event, modifiers.fromMaybe(0));
  SetEventTimestamp(event, Maybe<double>(timestamp));
  if (!SetMouseEventButton(event, button))
    return Response::InvalidParams("Invalid mouse button");

  event->x = x;
  event->y = y;
  event->windowX = x;
  event->windowY = y;
  event->globalX = x;
  event->globalY = y;
  event->clickCount = click_count.fromMaybe(0);
  event->pointerType = blink::WebPointerProperties::PointerType::Touch;

  if (!host_ || !host_->GetRenderWidgetHost())
    return Response::InternalError();

  if (event->type == blink::WebInputEvent::MouseWheel)
    host_->GetRenderWidgetHost()->ForwardWheelEvent(wheel_event);
  else
    host_->GetRenderWidgetHost()->ForwardMouseEvent(mouse_event);
  return Response::OK();
}

void InputHandler::SynthesizePinchGesture(
    int x,
    int y,
    double scale_factor,
    Maybe<int> relative_speed,
    Maybe<std::string> gesture_source_type,
    std::unique_ptr<SynthesizePinchGestureCallback> callback) {
  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  SyntheticPinchGestureParams gesture_params;
  const int kDefaultRelativeSpeed = 800;

  gesture_params.scale_factor = scale_factor;
  gesture_params.anchor = CssPixelsToPointF(x, y, page_scale_factor_);
  gesture_params.relative_pointer_speed_in_pixels_s =
      relative_speed.fromMaybe(kDefaultRelativeSpeed);

  if (!StringToGestureSourceType(
      std::move(gesture_source_type),
      gesture_params.gesture_source_type)) {
    callback->sendFailure(
        Response::InvalidParams("Unknown gestureSourceType"));
    return;
  }

  host_->GetRenderWidgetHost()->QueueSyntheticGesture(
      SyntheticGesture::Create(gesture_params),
      base::Bind(&SendSynthesizePinchGestureResponse,
                 base::Passed(std::move(callback))));
}

void InputHandler::SynthesizeScrollGesture(
    int x,
    int y,
    Maybe<int> x_distance,
    Maybe<int> y_distance,
    Maybe<int> x_overscroll,
    Maybe<int> y_overscroll,
    Maybe<bool> prevent_fling,
    Maybe<int> speed,
    Maybe<std::string> gesture_source_type,
    Maybe<int> repeat_count,
    Maybe<int> repeat_delay_ms,
    Maybe<std::string> interaction_marker_name,
    std::unique_ptr<SynthesizeScrollGestureCallback> callback) {
  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  SyntheticSmoothScrollGestureParams gesture_params;
  const bool kDefaultPreventFling = true;
  const int kDefaultSpeed = 800;

  gesture_params.anchor = CssPixelsToPointF(x, y, page_scale_factor_);
  gesture_params.prevent_fling =
      prevent_fling.fromMaybe(kDefaultPreventFling);
  gesture_params.speed_in_pixels_s = speed.fromMaybe(kDefaultSpeed);

  if (x_distance.fromJust() || y_distance.fromJust()) {
    gesture_params.distances.push_back(
        CssPixelsToVector2dF(x_distance.fromMaybe(0),
                             y_distance.fromMaybe(0), page_scale_factor_));
  }

  if (x_overscroll.isJust() || y_overscroll.isJust()) {
    gesture_params.distances.push_back(CssPixelsToVector2dF(
        -x_overscroll.fromMaybe(0), -y_overscroll.fromMaybe(0),
        page_scale_factor_));
  }

  if (!StringToGestureSourceType(
      std::move(gesture_source_type),
      gesture_params.gesture_source_type)) {
    callback->sendFailure(
        Response::InvalidParams("Unknown gestureSourceType"));
    return;
  }

  SynthesizeRepeatingScroll(
      gesture_params, repeat_count.fromMaybe(0),
      base::TimeDelta::FromMilliseconds(repeat_delay_ms.fromMaybe(250)),
      interaction_marker_name.fromMaybe(""), ++last_id_, std::move(callback));
}

void InputHandler::SynthesizeRepeatingScroll(
    SyntheticSmoothScrollGestureParams gesture_params,
    int repeat_count,
    base::TimeDelta repeat_delay,
    std::string interaction_marker_name,
    int id,
    std::unique_ptr<SynthesizeScrollGestureCallback> callback) {
  if (!interaction_marker_name.empty()) {
    // TODO(alexclarke): Can we move this elsewhere? It doesn't really fit here.
    TRACE_EVENT_COPY_ASYNC_BEGIN0("benchmark", interaction_marker_name.c_str(),
                                  id);
  }

  host_->GetRenderWidgetHost()->QueueSyntheticGesture(
      SyntheticGesture::Create(gesture_params),
      base::Bind(&InputHandler::OnScrollFinished, weak_factory_.GetWeakPtr(),
                 gesture_params, repeat_count, repeat_delay,
                 interaction_marker_name, id,
                 base::Passed(std::move(callback))));
}

void InputHandler::OnScrollFinished(
    SyntheticSmoothScrollGestureParams gesture_params,
    int repeat_count,
    base::TimeDelta repeat_delay,
    std::string interaction_marker_name,
    int id,
    std::unique_ptr<SynthesizeScrollGestureCallback> callback,
    SyntheticGesture::Result result) {
  if (!interaction_marker_name.empty()) {
    TRACE_EVENT_COPY_ASYNC_END0("benchmark", interaction_marker_name.c_str(),
                                id);
  }

  if (repeat_count > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&InputHandler::SynthesizeRepeatingScroll,
                   weak_factory_.GetWeakPtr(), gesture_params, repeat_count - 1,
                   repeat_delay, interaction_marker_name, id,
                   base::Passed(std::move(callback))),
        repeat_delay);
  } else {
    SendSynthesizeScrollGestureResponse(std::move(callback), result);
  }
}

void InputHandler::SynthesizeTapGesture(
    int x,
    int y,
    Maybe<int> duration,
    Maybe<int> tap_count,
    Maybe<std::string> gesture_source_type,
    std::unique_ptr<SynthesizeTapGestureCallback> callback) {
  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  SyntheticTapGestureParams gesture_params;
  const int kDefaultDuration = 50;
  const int kDefaultTapCount = 1;

  gesture_params.position = CssPixelsToPointF(x, y, page_scale_factor_);
  gesture_params.duration_ms = duration.fromMaybe(kDefaultDuration);

  if (!StringToGestureSourceType(
      std::move(gesture_source_type),
      gesture_params.gesture_source_type)) {
    callback->sendFailure(
        Response::InvalidParams("Unknown gestureSourceType"));
    return;
  }

  int count = tap_count.fromMaybe(kDefaultTapCount);
  if (!count) {
    callback->sendSuccess();
    return;
  }

  TapGestureResponse* response =
      new TapGestureResponse(std::move(callback), count);
  for (int i = 0; i < count; i++) {
    host_->GetRenderWidgetHost()->QueueSyntheticGesture(
        SyntheticGesture::Create(gesture_params),
        base::Bind(&TapGestureResponse::OnGestureResult,
                   base::Unretained(response)));
  }
}

}  // namespace protocol
}  // namespace content
