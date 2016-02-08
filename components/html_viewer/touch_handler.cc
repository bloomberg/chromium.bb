// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/touch_handler.h"

#include "components/mus/public/interfaces/input_events.mojom.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/gesture_detection/motion_event_generic.h"

namespace html_viewer {
namespace {

// TODO(rjkroege): Gesture recognition currently happens in the html_viewer.
// In phase2, it will be relocated to MUS. Update this code at that time.
void SetPropertiesFromEvent(const mus::mojom::Event& event,
                            ui::PointerProperties* properties) {
  if (event.pointer_data) {
    properties->id = event.pointer_data->pointer_id;
    if (event.pointer_data->location) {
      properties->x = event.pointer_data->location->x;
      properties->y = event.pointer_data->location->y;
      properties->raw_x = event.pointer_data->location->screen_x;
      properties->raw_y = event.pointer_data->location->screen_y;
    }
  }

  if (event.pointer_data && event.pointer_data->brush_data &&
      (event.pointer_data->kind == mus::mojom::PointerKind::TOUCH ||
       event.pointer_data->kind == mus::mojom::PointerKind::PEN)) {
    properties->pressure = event.pointer_data->brush_data->pressure;

    // TODO(rjkroege): vary orientation for width, height.
    properties->SetAxesAndOrientation(event.pointer_data->brush_data->width,
                                      event.pointer_data->brush_data->height,
                                      0.0);
  } else {
    if (event.flags & mus::mojom::kEventFlagLeftMouseButton ||
        event.flags & mus::mojom::kEventFlagMiddleMouseButton ||
        event.flags & mus::mojom::kEventFlagMiddleMouseButton) {
      properties->pressure = 0.5;
    } else {
      properties->pressure = 0.0;
    }
  }
  // TODO(sky): Add support for tool_type.
}

}  // namespace

TouchHandler::TouchHandler(blink::WebWidget* web_widget)
    : web_widget_(web_widget),
      gesture_provider_(ui::GetGestureProviderConfig(
                            ui::GestureProviderConfigType::CURRENT_PLATFORM),
                        this) {
}

TouchHandler::~TouchHandler() {
}

void TouchHandler::OnTouchEvent(const mus::mojom::Event& event) {
  if (!UpdateMotionEvent(event))
    return;

  SendMotionEventToGestureProvider();

  PostProcessMotionEvent(event);
}

void TouchHandler::OnGestureEvent(const ui::GestureEventData& gesture) {
  blink::WebGestureEvent web_gesture =
      CreateWebGestureEventFromGestureEventData(gesture);
  // TODO(jdduke): Remove this workaround after Android fixes UiAutomator to
  // stop providing shift meta values to synthetic MotionEvents. This prevents
  // unintended shift+click interpretation of all accessibility clicks.
  // See crbug.com/443247.
  if (web_gesture.type == blink::WebInputEvent::GestureTap &&
      web_gesture.modifiers == blink::WebInputEvent::ShiftKey) {
    web_gesture.modifiers = 0;
  }
  web_widget_->handleInputEvent(web_gesture);
}

bool TouchHandler::UpdateMotionEvent(const mus::mojom::Event& event) {
  ui::PointerProperties properties;
  SetPropertiesFromEvent(event, &properties);

  const base::TimeTicks timestamp(
      base::TimeTicks::FromInternalValue(event.time_stamp));
  if (current_motion_event_.get()) {
    current_motion_event_->set_unique_event_id(ui::GetNextTouchEventId());
    current_motion_event_->set_event_time(timestamp);
  }

  switch (event.action) {
    case mus::mojom::EventType::POINTER_DOWN:
      if (!current_motion_event_.get()) {
        current_motion_event_.reset(new ui::MotionEventGeneric(
            ui::MotionEvent::ACTION_DOWN, timestamp, properties));
      } else {
        const int index =
            current_motion_event_->FindPointerIndexOfId(properties.id);
        if (index != -1) {
          DVLOG(1) << "pointer down and pointer already down id="
                   << properties.id;
          return false;
        }
        current_motion_event_->PushPointer(properties);
        current_motion_event_->set_action(ui::MotionEvent::ACTION_POINTER_DOWN);
        current_motion_event_->set_action_index(static_cast<int>(index));
      }
      return true;

    case mus::mojom::EventType::POINTER_UP: {
      if (!current_motion_event_.get()) {
        DVLOG(1) << "pointer up with no event, id=" << properties.id;
        return false;
      }
      const int index =
          current_motion_event_->FindPointerIndexOfId(properties.id);
      if (index == -1) {
        DVLOG(1) << "pointer up and pointer not down id=" << properties.id;
        return false;
      }
      current_motion_event_->pointer(index) = properties;
      current_motion_event_->set_action(
          current_motion_event_->GetPointerCount() == 1
              ? ui::MotionEvent::ACTION_UP
              : ui::MotionEvent::ACTION_POINTER_UP);
      current_motion_event_->set_action_index(static_cast<int>(index));
      return true;
    }

    case mus::mojom::EventType::POINTER_MOVE: {
      if (!current_motion_event_.get()) {
        DVLOG(1) << "pointer move with no event, id=" << properties.id;
        return false;
      }
      const int index =
          current_motion_event_->FindPointerIndexOfId(properties.id);
      if (index == -1) {
        DVLOG(1) << "pointer move and pointer not down id=" << properties.id;
        return false;
      }
      current_motion_event_->pointer(index) = properties;
      current_motion_event_->set_action(ui::MotionEvent::ACTION_MOVE);
      current_motion_event_->set_action_index(static_cast<int>(index));
      return true;
    }

    case mus::mojom::EventType::POINTER_CANCEL: {
      if (!current_motion_event_.get()) {
        DVLOG(1) << "canel with no event, id=" << properties.id;
        return false;
      }
      const int index =
          current_motion_event_->FindPointerIndexOfId(properties.id);
      if (index == -1) {
        DVLOG(1) << "cancel and pointer not down id=" << properties.id;
        return false;
      }
      current_motion_event_->pointer(index) = properties;
      current_motion_event_->set_action(ui::MotionEvent::ACTION_CANCEL);
      current_motion_event_->set_action_index(0);
      return true;
    }

    default:
      NOTREACHED();
  }
  return false;
}

void TouchHandler::SendMotionEventToGestureProvider() {
  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(*current_motion_event_);
  if (!result.succeeded)
    return;

  blink::WebTouchEvent web_event = ui::CreateWebTouchEventFromMotionEvent(
      *current_motion_event_, result.moved_beyond_slop_region);
  gesture_provider_.OnTouchEventAck(web_event.uniqueTouchEventId,
                                    web_widget_->handleInputEvent(web_event) !=
                                        blink::WebInputEventResult::NotHandled);
}

void TouchHandler::PostProcessMotionEvent(const mus::mojom::Event& event) {
  switch (event.action) {
    case mus::mojom::EventType::POINTER_UP: {
      if (event.pointer_data) {
        const int index = current_motion_event_->FindPointerIndexOfId(
            event.pointer_data->pointer_id);
        current_motion_event_->RemovePointerAt(index);
      }
      if (current_motion_event_->GetPointerCount() == 0)
        current_motion_event_.reset();
      break;
    }

    case mus::mojom::EventType::POINTER_CANCEL:
      current_motion_event_.reset();
      break;

    default:
      break;
  }
}

}  // namespace html_viewer
