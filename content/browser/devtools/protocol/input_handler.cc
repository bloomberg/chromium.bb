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
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/native_input_event_builder.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"

namespace content {
namespace protocol {

namespace {

gfx::PointF CssPixelsToPointF(double x, double y, float page_scale_factor) {
  return gfx::PointF(x * page_scale_factor, y * page_scale_factor);
}

gfx::Vector2dF CssPixelsToVector2dF(double x,
                                    double y,
                                    float page_scale_factor) {
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

int GetEventModifiers(int modifiers, bool auto_repeat, bool is_keypad) {
  int result = 0;
  if (auto_repeat)
    result |= blink::WebInputEvent::kIsAutoRepeat;
  if (is_keypad)
    result |= blink::WebInputEvent::kIsKeyPad;

  if (modifiers & 1)
    result |= blink::WebInputEvent::kAltKey;
  if (modifiers & 2)
    result |= blink::WebInputEvent::kControlKey;
  if (modifiers & 4)
    result |= blink::WebInputEvent::kMetaKey;
  if (modifiers & 8)
    result |= blink::WebInputEvent::kShiftKey;
  return result;
}

base::TimeTicks GetEventTimeTicks(Maybe<double> timestamp) {
  // Convert timestamp, in seconds since unix epoch, to an event timestamp
  // which is time ticks since platform start time.
  return timestamp.isJust()
             ? base::TimeDelta::FromSecondsD(timestamp.fromJust()) +
                   base::TimeTicks::UnixEpoch()
             : base::TimeTicks::Now();
}

double GetEventTimestamp(Maybe<double> timestamp) {
  return (GetEventTimeTicks(std::move(timestamp)) - base::TimeTicks())
      .InSecondsF();
}

bool SetKeyboardEventText(blink::WebUChar* to, Maybe<std::string> from) {
  if (!from.isJust())
    return true;

  base::string16 text16 = base::UTF8ToUTF16(from.fromJust());
  if (text16.size() > blink::WebKeyboardEvent::kTextLengthCap)
    return false;

  for (size_t i = 0; i < text16.size(); ++i)
    to[i] = text16[i];
  return true;
}

bool GetMouseEventButton(const std::string& button,
                         blink::WebPointerProperties::Button* event_button,
                         int* event_modifiers) {
  if (button.empty())
    return true;

  if (button == Input::DispatchMouseEvent::ButtonEnum::None) {
    *event_button = blink::WebMouseEvent::Button::kNoButton;
  } else if (button == Input::DispatchMouseEvent::ButtonEnum::Left) {
    *event_button = blink::WebMouseEvent::Button::kLeft;
    *event_modifiers = blink::WebInputEvent::kLeftButtonDown;
  } else if (button == Input::DispatchMouseEvent::ButtonEnum::Middle) {
    *event_button = blink::WebMouseEvent::Button::kMiddle;
    *event_modifiers = blink::WebInputEvent::kMiddleButtonDown;
  } else if (button == Input::DispatchMouseEvent::ButtonEnum::Right) {
    *event_button = blink::WebMouseEvent::Button::kRight;
    *event_modifiers = blink::WebInputEvent::kRightButtonDown;
  } else {
    return false;
  }
  return true;
}

blink::WebInputEvent::Type GetMouseEventType(const std::string& type) {
  if (type == Input::DispatchMouseEvent::TypeEnum::MousePressed)
    return blink::WebInputEvent::kMouseDown;
  if (type == Input::DispatchMouseEvent::TypeEnum::MouseReleased)
    return blink::WebInputEvent::kMouseUp;
  if (type == Input::DispatchMouseEvent::TypeEnum::MouseMoved)
    return blink::WebInputEvent::kMouseMove;
  if (type == Input::DispatchMouseEvent::TypeEnum::MouseWheel)
    return blink::WebInputEvent::kMouseWheel;
  return blink::WebInputEvent::kUndefined;
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
    : DevToolsDomainHandler(Input::Metainfo::domainName),
      host_(nullptr),
      input_queued_(false),
      page_scale_factor_(1.0),
      last_id_(0),
      weak_factory_(this) {
}

InputHandler::~InputHandler() {
}

// static
std::vector<InputHandler*> InputHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return DevToolsSession::HandlersForAgentHost<InputHandler>(
      host, Input::Metainfo::domainName);
}

void InputHandler::SetRenderFrameHost(RenderFrameHostImpl* host) {
  ClearPendingKeyAndMouseCallbacks();
  if (host_) {
    host_->GetRenderWidgetHost()->RemoveInputEventObserver(this);
    if (ignore_input_events_)
      host_->GetRenderWidgetHost()->SetIgnoreInputEvents(false);
  }
  host_ = host;
  if (host) {
    host->GetRenderWidgetHost()->AddInputEventObserver(this);
    if (ignore_input_events_)
      host_->GetRenderWidgetHost()->SetIgnoreInputEvents(true);
  }
}

void InputHandler::OnInputEvent(const blink::WebInputEvent& event) {
  input_queued_ = true;
}

void InputHandler::OnInputEventAck(const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::IsKeyboardEventType(event.GetType()) &&
      !pending_key_callbacks_.empty()) {
    pending_key_callbacks_.front()->sendSuccess();
    pending_key_callbacks_.pop_front();
    return;
  }
  if ((blink::WebInputEvent::IsMouseEventType(event.GetType()) ||
       event.GetType() == blink::WebInputEvent::kMouseWheel) &&
      !pending_mouse_callbacks_.empty()) {
    pending_mouse_callbacks_.front()->sendSuccess();
    pending_mouse_callbacks_.pop_front();
    return;
  }
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
  ClearPendingKeyAndMouseCallbacks();
  if (host_) {
    host_->GetRenderWidgetHost()->RemoveInputEventObserver(this);
    if (ignore_input_events_)
      host_->GetRenderWidgetHost()->SetIgnoreInputEvents(false);
  }
  ignore_input_events_ = false;
  return Response::OK();
}

void InputHandler::DispatchKeyEvent(
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
    Maybe<bool> is_system_key,
    std::unique_ptr<DispatchKeyEventCallback> callback) {
  blink::WebInputEvent::Type web_event_type;

  if (type == Input::DispatchKeyEvent::TypeEnum::KeyDown) {
    web_event_type = blink::WebInputEvent::kKeyDown;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::KeyUp) {
    web_event_type = blink::WebInputEvent::kKeyUp;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::Char) {
    web_event_type = blink::WebInputEvent::kChar;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::RawKeyDown) {
    web_event_type = blink::WebInputEvent::kRawKeyDown;
  } else {
    callback->sendFailure(Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str())));
    return;
  }

  NativeWebKeyboardEvent event(
      web_event_type,
      GetEventModifiers(modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers),
                        auto_repeat.fromMaybe(false),
                        is_keypad.fromMaybe(false)),
      GetEventTimeTicks(std::move(timestamp)));

  if (!SetKeyboardEventText(event.text, std::move(text))) {
    callback->sendFailure(Response::InvalidParams("Invalid 'text' parameter"));
    return;
  }
  if (!SetKeyboardEventText(event.unmodified_text,
                            std::move(unmodified_text))) {
    callback->sendFailure(
        Response::InvalidParams("Invalid 'unmodifiedText' parameter"));
    return;
  }

  if (windows_virtual_key_code.isJust())
    event.windows_key_code = windows_virtual_key_code.fromJust();
  if (native_virtual_key_code.isJust())
    event.native_key_code = native_virtual_key_code.fromJust();
  if (is_system_key.isJust())
    event.is_system_key = is_system_key.fromJust();

  if (code.isJust()) {
    event.dom_code = static_cast<int>(
        ui::KeycodeConverter::CodeStringToDomCode(code.fromJust()));
  }

  if (key.isJust()) {
    event.dom_key = static_cast<int>(
        ui::KeycodeConverter::KeyStringToDomKey(key.fromJust()));
  }

  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  // We do not pass events to browser if there is no native key event
  // due to Mac needing the actual os_event.
  if (event.native_key_code)
    event.os_event = NativeInputEventBuilder::CreateEvent(event);
  else
    event.skip_in_browser = true;

  host_->GetRenderWidgetHost()->Focus();
  input_queued_ = false;
  pending_key_callbacks_.push_back(std::move(callback));
  host_->GetRenderWidgetHost()->ForwardKeyboardEvent(event);
  if (!input_queued_) {
    pending_key_callbacks_.back()->sendSuccess();
    pending_key_callbacks_.pop_back();
  }
}

void InputHandler::DispatchMouseEvent(
    const std::string& event_type,
    double x,
    double y,
    Maybe<int> maybe_modifiers,
    Maybe<double> maybe_timestamp,
    Maybe<std::string> maybe_button,
    Maybe<int> click_count,
    Maybe<double> delta_x,
    Maybe<double> delta_y,
    std::unique_ptr<DispatchMouseEventCallback> callback) {
  blink::WebInputEvent::Type type = GetMouseEventType(event_type);
  if (type == blink::WebInputEvent::kUndefined) {
    callback->sendFailure(Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", event_type.c_str())));
    return;
  }

  blink::WebPointerProperties::Button button =
      blink::WebPointerProperties::Button::kNoButton;
  int button_modifiers = 0;
  if (!GetMouseEventButton(maybe_button.fromMaybe(""), &button,
                           &button_modifiers)) {
    callback->sendFailure(Response::InvalidParams("Invalid mouse button"));
    return;
  }

  int modifiers = GetEventModifiers(
      maybe_modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers), false,
      false);
  modifiers |= button_modifiers;
  double timestamp = GetEventTimestamp(std::move(maybe_timestamp));

  std::unique_ptr<blink::WebMouseEvent, ui::WebInputEventDeleter> mouse_event;
  blink::WebMouseWheelEvent* wheel_event = nullptr;

  if (type == blink::WebInputEvent::kMouseWheel) {
    wheel_event = new blink::WebMouseWheelEvent(type, modifiers, timestamp);
    mouse_event.reset(wheel_event);
    if (!delta_x.isJust() || !delta_y.isJust()) {
      callback->sendFailure(Response::InvalidParams(
          "'deltaX' and 'deltaY' are expected for mouseWheel event"));
      return;
    }
    wheel_event->delta_x = static_cast<float>(-delta_x.fromJust());
    wheel_event->delta_y = static_cast<float>(-delta_y.fromJust());
    wheel_event->dispatch_type = blink::WebInputEvent::kBlocking;
  } else {
    mouse_event.reset(new blink::WebMouseEvent(type, modifiers, timestamp));
  }

  mouse_event->button = button;
  mouse_event->SetPositionInWidget(x * page_scale_factor_,
                                   y * page_scale_factor_);
  mouse_event->SetPositionInScreen(x * page_scale_factor_,
                                   y * page_scale_factor_);
  mouse_event->click_count = click_count.fromMaybe(0);
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kMouse;

  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  host_->GetRenderWidgetHost()->Focus();
  input_queued_ = false;
  pending_mouse_callbacks_.push_back(std::move(callback));
  if (wheel_event)
    host_->GetRenderWidgetHost()->ForwardWheelEvent(*wheel_event);
  else
    host_->GetRenderWidgetHost()->ForwardMouseEvent(*mouse_event);
  if (!input_queued_) {
    pending_mouse_callbacks_.back()->sendSuccess();
    pending_mouse_callbacks_.pop_back();
  }
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
  blink::WebInputEvent::Type event_type;
  if (type == Input::EmulateTouchFromMouseEvent::TypeEnum::MouseWheel) {
    event_type = blink::WebInputEvent::kMouseWheel;
    if (!delta_x.isJust() || !delta_y.isJust()) {
      return Response::InvalidParams(
          "'deltaX' and 'deltaY' are expected for mouseWheel event");
    }
  } else {
    event_type = GetMouseEventType(type);
    if (event_type == blink::WebInputEvent::kUndefined) {
      return Response::InvalidParams(
          base::StringPrintf("Unexpected event type '%s'", type.c_str()));
    }
  }

  blink::WebPointerProperties::Button event_button =
      blink::WebPointerProperties::Button::kNoButton;
  int button_modifiers = 0;
  if (!GetMouseEventButton(button, &event_button, &button_modifiers))
    return Response::InvalidParams("Invalid mouse button");

  ui::WebScopedInputEvent event;
  blink::WebMouseWheelEvent* wheel_event = nullptr;
  blink::WebMouseEvent* mouse_event = nullptr;
  if (type == Input::EmulateTouchFromMouseEvent::TypeEnum::MouseWheel) {
    wheel_event = new blink::WebMouseWheelEvent(
        event_type,
        GetEventModifiers(
            modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers), false,
            false) |
            button_modifiers,
        GetEventTimestamp(timestamp));
    mouse_event = wheel_event;
    event.reset(wheel_event);
    wheel_event->delta_x = static_cast<float>(delta_x.fromJust());
    wheel_event->delta_y = static_cast<float>(delta_y.fromJust());
  } else {
    mouse_event = new blink::WebMouseEvent(
        event_type,
        GetEventModifiers(
            modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers), false,
            false) |
            button_modifiers,
        GetEventTimestamp(timestamp));
    event.reset(mouse_event);
  }

  mouse_event->SetPositionInWidget(x, y);
  mouse_event->button = event_button;
  mouse_event->SetPositionInScreen(x, y);
  mouse_event->click_count = click_count.fromMaybe(0);
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kTouch;

  if (!host_ || !host_->GetRenderWidgetHost())
    return Response::InternalError();

  if (wheel_event)
    host_->GetRenderWidgetHost()->ForwardWheelEvent(*wheel_event);
  else
    host_->GetRenderWidgetHost()->ForwardMouseEvent(*mouse_event);
  return Response::OK();
}

Response InputHandler::SetIgnoreInputEvents(bool ignore) {
  ignore_input_events_ = ignore;
  if (host_)
    host_->GetRenderWidgetHost()->SetIgnoreInputEvents(ignore);
  return Response::OK();
}

void InputHandler::SynthesizePinchGesture(
    double x,
    double y,
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
  if (!PointIsWithinContents(gesture_params.anchor)) {
    callback->sendFailure(Response::InvalidParams("Position out of bounds"));
    return;
  }

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
    double x,
    double y,
    Maybe<double> x_distance,
    Maybe<double> y_distance,
    Maybe<double> x_overscroll,
    Maybe<double> y_overscroll,
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
  if (!PointIsWithinContents(gesture_params.anchor)) {
    callback->sendFailure(Response::InvalidParams("Position out of bounds"));
    return;
  }

  gesture_params.prevent_fling =
      prevent_fling.fromMaybe(kDefaultPreventFling);
  gesture_params.speed_in_pixels_s = speed.fromMaybe(kDefaultSpeed);

  if (x_distance.isJust() || y_distance.isJust()) {
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
    double x,
    double y,
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
  if (!PointIsWithinContents(gesture_params.position)) {
    callback->sendFailure(Response::InvalidParams("Position out of bounds"));
    return;
  }

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

void InputHandler::ClearPendingKeyAndMouseCallbacks() {
  for (auto& callback : pending_key_callbacks_)
    callback->sendSuccess();
  pending_key_callbacks_.clear();
  for (auto& callback : pending_mouse_callbacks_)
    callback->sendSuccess();
  pending_mouse_callbacks_.clear();
}

bool InputHandler::PointIsWithinContents(gfx::PointF point) const {
  gfx::Rect bounds = host_->GetView()->GetViewBounds();
  bounds -= bounds.OffsetFromOrigin();  // Translate the bounds to (0,0).
  return bounds.Contains(point.x(), point.y());
}

}  // namespace protocol
}  // namespace content
