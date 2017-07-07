// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/legacy_ipc_widget_input_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/legacy_input_router_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/input_messages.h"

namespace content {

namespace {
std::vector<blink::WebCompositionUnderline> ConvertToBlinkUnderline(
    const std::vector<ui::CompositionUnderline>& ui_underlines) {
  std::vector<blink::WebCompositionUnderline> underlines;
  for (const auto& underline : ui_underlines) {
    underlines.emplace_back(blink::WebCompositionUnderline(
        underline.start_offset, underline.end_offset, underline.color,
        underline.thick, underline.background_color));
  }
  return underlines;
}

}  // namespace

LegacyIPCWidgetInputHandler::LegacyIPCWidgetInputHandler(
    LegacyInputRouterImpl* input_router)
    : input_router_(input_router) {}

LegacyIPCWidgetInputHandler::~LegacyIPCWidgetInputHandler() {}

void LegacyIPCWidgetInputHandler::SetFocus(bool focused) {
  SendInput(base::MakeUnique<InputMsg_SetFocus>(input_router_->routing_id(),
                                                focused));
}

void LegacyIPCWidgetInputHandler::MouseCaptureLost() {}

void LegacyIPCWidgetInputHandler::SetEditCommandsForNextKeyEvent(
    const std::vector<EditCommand>& commands) {
  SendInput(base::MakeUnique<InputMsg_SetEditCommandsForNextKeyEvent>(
      input_router_->routing_id(), commands));
}

void LegacyIPCWidgetInputHandler::CursorVisibilityChanged(bool visible) {
  SendInput(base::MakeUnique<InputMsg_CursorVisibilityChange>(
      input_router_->routing_id(), visible));
}

void LegacyIPCWidgetInputHandler::ImeSetComposition(
    const base::string16& text,
    const std::vector<ui::CompositionUnderline>& ui_underlines,
    const gfx::Range& range,
    int32_t start,
    int32_t end) {
  std::vector<blink::WebCompositionUnderline> underlines =
      ConvertToBlinkUnderline(ui_underlines);
  SendInput(base::MakeUnique<InputMsg_ImeSetComposition>(
      input_router_->routing_id(), text, underlines, range, start, end));
}

void LegacyIPCWidgetInputHandler::ImeCommitText(
    const base::string16& text,
    const std::vector<ui::CompositionUnderline>& ui_underlines,
    const gfx::Range& range,
    int32_t relative_cursor_position) {
  std::vector<blink::WebCompositionUnderline> underlines =
      ConvertToBlinkUnderline(ui_underlines);
  SendInput(base::MakeUnique<InputMsg_ImeCommitText>(
      input_router_->routing_id(), text, underlines, range,
      relative_cursor_position));
}

void LegacyIPCWidgetInputHandler::ImeFinishComposingText(bool keep_selection) {
  SendInput(base::MakeUnique<InputMsg_ImeFinishComposingText>(
      input_router_->routing_id(), keep_selection));
}
void LegacyIPCWidgetInputHandler::RequestTextInputStateUpdate() {
#if defined(OS_ANDROID)
  SendInput(base::MakeUnique<InputMsg_RequestTextInputStateUpdate>(
      input_router_->routing_id()));
#endif
}

void LegacyIPCWidgetInputHandler::RequestCompositionUpdates(
    bool immediate_request,
    bool monitor_request) {
  SendInput(base::MakeUnique<InputMsg_RequestCompositionUpdates>(
      input_router_->routing_id(), immediate_request, monitor_request));
}

void LegacyIPCWidgetInputHandler::SendInput(
    std::unique_ptr<IPC::Message> message) {
  input_router_->SendInput(std::move(message));
}

void LegacyIPCWidgetInputHandler::DispatchEvent(
    std::unique_ptr<content::InputEvent> event,
    DispatchEventCallback callback) {
  // We only expect these events to be called with the mojo enabled input
  // channel. The LegacyInputRouterImpl will handle sending the events
  // directly.
  NOTREACHED();
}

void LegacyIPCWidgetInputHandler::DispatchNonBlockingEvent(
    std::unique_ptr<content::InputEvent> event) {
  // We only expect these events to be called with the mojo enabled input
  // channel. The LegacyInputRouterImpl will handle sending the events
  // directly.
  NOTREACHED();
}

}  // namespace content
