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

blink::WebImeTextSpan::Type ConvertUiImeTextSpanTypeToBlinkType(
    ui::ImeTextSpan::Type type) {
  switch (type) {
    case ui::ImeTextSpan::Type::kComposition:
      return blink::WebImeTextSpan::Type::kComposition;
    case ui::ImeTextSpan::Type::kSuggestion:
      return blink::WebImeTextSpan::Type::kSuggestion;
  }

  NOTREACHED();
  return blink::WebImeTextSpan::Type::kComposition;
}

std::vector<blink::WebImeTextSpan> ConvertToBlinkImeTextSpan(
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  std::vector<blink::WebImeTextSpan> ime_text_spans;
  for (const auto& ime_text_span : ui_ime_text_spans) {
    ime_text_spans.emplace_back(blink::WebImeTextSpan(
        ConvertUiImeTextSpanTypeToBlinkType(ime_text_span.type),
        ime_text_span.start_offset, ime_text_span.end_offset,
        ime_text_span.underline_color, ime_text_span.thick,
        ime_text_span.background_color,
        ime_text_span.suggestion_highlight_color, ime_text_span.suggestions));
  }
  return ime_text_spans;
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
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans,
    const gfx::Range& range,
    int32_t start,
    int32_t end) {
  std::vector<blink::WebImeTextSpan> ime_text_spans =
      ConvertToBlinkImeTextSpan(ui_ime_text_spans);
  SendInput(base::MakeUnique<InputMsg_ImeSetComposition>(
      input_router_->routing_id(), text, ime_text_spans, range, start, end));
}

void LegacyIPCWidgetInputHandler::ImeCommitText(
    const base::string16& text,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans,
    const gfx::Range& range,
    int32_t relative_cursor_position) {
  std::vector<blink::WebImeTextSpan> ime_text_spans =
      ConvertToBlinkImeTextSpan(ui_ime_text_spans);
  SendInput(base::MakeUnique<InputMsg_ImeCommitText>(
      input_router_->routing_id(), text, ime_text_spans, range,
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
