// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/touch_handler.h"

#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/gesture_detection/motion_event_generic.h"
#include "ui/mojo/events/input_events.mojom.h"

namespace html_viewer {
namespace {

void SetPropertiesFromEvent(const mojo::Event& event,
                            ui::PointerProperties* properties) {
  properties->id = event.pointer_data->pointer_id;
  properties->x = event.pointer_data->x;
  properties->y = event.pointer_data->y;
  properties->raw_x = event.pointer_data->screen_x;
  properties->raw_y = event.pointer_data->screen_y;
  properties->pressure = event.pointer_data->pressure;
  properties->SetAxesAndOrientation(event.pointer_data->radius_major,
                                    event.pointer_data->radius_minor,
                                    event.pointer_data->orientation);
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

void TouchHandler::OnTouchEvent(const mojo::Event& event) {
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

bool TouchHandler::UpdateMotionEvent(const mojo::Event& event) {
  ui::PointerProperties properties;
  SetPropertiesFromEvent(event, &properties);

  const base::TimeTicks timestamp(
      base::TimeTicks::FromInternalValue(event.time_stamp));
  if (current_motion_event_.get()) {
    current_motion_event_->set_unique_event_id(ui::GetNextTouchEventId());
    current_motion_event_->set_event_time(timestamp);
  }

  switch (event.action) {
    case mojo::EVENT_TYPE_POINTER_DOWN:
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

    case mojo::EVENT_TYPE_POINTER_UP: {
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

    case mojo::EVENT_TYPE_POINTER_MOVE: {
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

    case mojo::EVENT_TYPE_POINTER_CANCEL: {
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
      *current_motion_event_, result.did_generate_scroll);
  gesture_provider_.OnTouchEventAck(web_event.uniqueTouchEventId,
                                    web_widget_->handleInputEvent(web_event));
}

void TouchHandler::PostProcessMotionEvent(const mojo::Event& event) {
  switch (event.action) {
    case mojo::EVENT_TYPE_POINTER_UP: {
      const int index = current_motion_event_->FindPointerIndexOfId(
          event.pointer_data->pointer_id);
      current_motion_event_->RemovePointerAt(index);
      if (current_motion_event_->GetPointerCount() == 0)
        current_motion_event_.reset();
      break;
    }

    case mojo::EVENT_TYPE_POINTER_CANCEL:
      current_motion_event_.reset();
      break;

    default:
      break;
  }
}

}  // namespace html_viewer
