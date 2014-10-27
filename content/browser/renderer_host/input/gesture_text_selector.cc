// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/gesture_text_selector.h"

#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_detector.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/gesture_detection/motion_event.h"

using ui::GestureDetector;
using ui::MotionEvent;

namespace content {
namespace {
scoped_ptr<GestureDetector> CreateGestureDetector(
    ui::GestureListener* listener) {
  GestureDetector::Config config =
      ui::DefaultGestureProviderConfig().gesture_detector_config;

  ui::DoubleTapListener* null_double_tap_listener = nullptr;

  // Doubletap, showpress and longpress detection are not required, and
  // should be explicitly disabled for efficiency.
  scoped_ptr<ui::GestureDetector> detector(
      new ui::GestureDetector(config, listener, null_double_tap_listener));
  detector->set_longpress_enabled(false);
  detector->set_showpress_enabled(false);

  return detector.Pass();
}

}  // namespace

GestureTextSelector::GestureTextSelector(GestureTextSelectorClient* client)
    : client_(client),
      text_selection_triggered_(false),
      secondary_button_pressed_(false),
      anchor_x_(0.0f),
      anchor_y_(0.0f) {
  DCHECK(client);
}

GestureTextSelector::~GestureTextSelector() {
}

bool GestureTextSelector::OnTouchEvent(const MotionEvent& event) {
  if (event.GetAction() == MotionEvent::ACTION_DOWN) {
    // Only trigger selection on ACTION_DOWN to prevent partial touch or gesture
    // sequences from being forwarded.
    text_selection_triggered_ = ShouldStartTextSelection(event);
    secondary_button_pressed_ =
        event.GetButtonState() == MotionEvent::BUTTON_SECONDARY;
    anchor_x_ = event.GetX();
    anchor_y_ = event.GetY();
  }

  if (!text_selection_triggered_)
    return false;

  if (event.GetAction() == MotionEvent::ACTION_MOVE) {
    secondary_button_pressed_ =
        event.GetButtonState() == MotionEvent::BUTTON_SECONDARY;
    if (!secondary_button_pressed_) {
      anchor_x_ = event.GetX();
      anchor_y_ = event.GetY();
    }
  }

  if (!gesture_detector_)
    gesture_detector_ = CreateGestureDetector(this);

  gesture_detector_->OnTouchEvent(event);

  // Always return true, even if |gesture_detector_| technically doesn't
  // consume the event, to prevent a partial touch stream from being forwarded.
  return true;
}

bool GestureTextSelector::OnSingleTapUp(const MotionEvent& e) {
  DCHECK(text_selection_triggered_);
  client_->LongPress(e.GetEventTime(), e.GetX(), e.GetY());
  return true;
}

bool GestureTextSelector::OnScroll(const MotionEvent& e1,
                                   const MotionEvent& e2,
                                   float distance_x,
                                   float distance_y) {
  DCHECK(text_selection_triggered_);

  // Return if Stylus button is not pressed.
  if (!secondary_button_pressed_)
    return true;

  // TODO(changwan): check if we can show handles after the scroll finishes
  // instead. Currently it is not possible as ShowSelectionHandles should
  // be called before we change the selection.
  client_->ShowSelectionHandlesAutomatically();
  client_->SelectRange(anchor_x_, anchor_y_, e2.GetX(), e2.GetY());
  return true;
}

// static
bool GestureTextSelector::ShouldStartTextSelection(const MotionEvent& event) {
  DCHECK_GT(event.GetPointerCount(), 0u);
  // Currently we are supporting stylus-only cases.
  const bool is_stylus = event.GetToolType(0) == MotionEvent::TOOL_TYPE_STYLUS;
  const bool is_only_secondary_button_pressed =
      event.GetButtonState() == MotionEvent::BUTTON_SECONDARY;
  return is_stylus && is_only_secondary_button_pressed;
}

}  // namespace content
