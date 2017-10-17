// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_widget_input_handler.h"

#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

MockWidgetInputHandler::MockWidgetInputHandler() : binding_(this) {}

MockWidgetInputHandler::MockWidgetInputHandler(
    mojom::WidgetInputHandlerRequest request,
    mojom::WidgetInputHandlerHostPtr host)
    : binding_(this, std::move(request)), host_(std::move(host)) {}

MockWidgetInputHandler::~MockWidgetInputHandler() {}

void MockWidgetInputHandler::SetFocus(bool focused) {}

void MockWidgetInputHandler::MouseCaptureLost() {}

void MockWidgetInputHandler::SetEditCommandsForNextKeyEvent(
    const std::vector<content::EditCommand>& commands) {
  edit_commands_ = commands;
}

void MockWidgetInputHandler::CursorVisibilityChanged(bool visible) {}

void MockWidgetInputHandler::ImeSetComposition(
    const base::string16& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& range,
    int32_t start,
    int32_t end) {}

void MockWidgetInputHandler::ImeCommitText(
    const base::string16& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& range,
    int32_t relative_cursor_position) {}

void MockWidgetInputHandler::ImeFinishComposingText(bool keep_selection) {}

void MockWidgetInputHandler::RequestTextInputStateUpdate() {}

void MockWidgetInputHandler::RequestCompositionUpdates(bool immediate_request,
                                                       bool monitor_request) {}

void MockWidgetInputHandler::DispatchEvent(
    std::unique_ptr<content::InputEvent> event,
    DispatchEventCallback callback) {
  dispatched_events_.emplace_back(
      DispatchedEvent(std::move(event), std::move(callback)));
}

void MockWidgetInputHandler::DispatchNonBlockingEvent(
    std::unique_ptr<content::InputEvent> event) {
  dispatched_events_.emplace_back(
      DispatchedEvent(std::move(event), DispatchEventCallback()));
}

std::vector<MockWidgetInputHandler::DispatchedEvent>
MockWidgetInputHandler::GetAndResetDispatchedEvents() {
  std::vector<DispatchedEvent> dispatched_events;
  dispatched_events_.swap(dispatched_events);
  return dispatched_events;
}

std::vector<content::EditCommand>
MockWidgetInputHandler::GetAndResetEditCommands() {
  std::vector<content::EditCommand> edit_commands;
  edit_commands_.swap(edit_commands);
  return edit_commands;
}

MockWidgetInputHandler::DispatchedEvent::DispatchedEvent(
    DispatchedEvent&& other)
    : event_(std::move(other.event_)), callback_(std::move(other.callback_)) {}

MockWidgetInputHandler::DispatchedEvent::DispatchedEvent(
    std::unique_ptr<content::InputEvent> event,
    DispatchEventCallback callback)
    : event_(std::move(event)), callback_(std::move(callback)) {}

MockWidgetInputHandler::DispatchedEvent::~DispatchedEvent() {}

}  // namespace content
