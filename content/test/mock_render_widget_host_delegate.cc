// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_render_widget_host_delegate.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/display/screen.h"

namespace content {

MockRenderWidgetHostDelegate::MockRenderWidgetHostDelegate()
    : text_input_manager_(false /* should_do_learning */) {}

MockRenderWidgetHostDelegate::~MockRenderWidgetHostDelegate() = default;

void MockRenderWidgetHostDelegate::ResizeDueToAutoResize(
    RenderWidgetHostImpl* render_widget_host,
    const gfx::Size& new_size) {}

KeyboardEventProcessingResult
MockRenderWidgetHostDelegate::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  last_event_ = std::make_unique<NativeWebKeyboardEvent>(event);
  return pre_handle_keyboard_event_result_;
}

void MockRenderWidgetHostDelegate::ExecuteEditCommand(
    const std::string& command,
    const base::Optional<base::string16>& value) {}

void MockRenderWidgetHostDelegate::Cut() {}

void MockRenderWidgetHostDelegate::Copy() {}

void MockRenderWidgetHostDelegate::Paste() {}

void MockRenderWidgetHostDelegate::SelectAll() {}

void MockRenderWidgetHostDelegate::CreateInputEventRouter() {
  rwh_input_event_router_ =
      std::make_unique<RenderWidgetHostInputEventRouter>();
}

RenderWidgetHostInputEventRouter*
MockRenderWidgetHostDelegate::GetInputEventRouter() {
  return rwh_input_event_router_.get();
}

RenderWidgetHostImpl* MockRenderWidgetHostDelegate::GetFocusedRenderWidgetHost(
    RenderWidgetHostImpl* widget_host) {
  return !!focused_widget_ ? focused_widget_ : widget_host;
}

void MockRenderWidgetHostDelegate::SendScreenRects() {
  if (rwh_)
    rwh_->SendScreenRects();
}

TextInputManager* MockRenderWidgetHostDelegate::GetTextInputManager() {
  return &text_input_manager_;
}

bool MockRenderWidgetHostDelegate::IsFullscreenForCurrentTab() {
  return is_fullscreen_;
}

RenderViewHostDelegateView* MockRenderWidgetHostDelegate::GetDelegateView() {
  return &rvh_delegate_view_;
}

}  // namespace content
