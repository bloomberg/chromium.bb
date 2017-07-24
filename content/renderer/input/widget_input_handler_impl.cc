// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/widget_input_handler_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "content/common/input_messages.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/widget_input_handler_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebCoalescedInputEvent.h"
#include "third_party/WebKit/public/platform/WebKeyboardEvent.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

namespace {

std::vector<blink::WebCompositionUnderline>
ConvertUIUnderlinesToBlinkUnderlines(
    const std::vector<ui::CompositionUnderline>& ui_underlines) {
  std::vector<blink::WebCompositionUnderline> underlines;
  for (const auto& underline : ui_underlines) {
    blink::WebCompositionUnderline blink_underline(
        underline.start_offset, underline.end_offset, underline.color,
        underline.thick, underline.background_color);
    underlines.push_back(blink_underline);
  }
  return underlines;
}
}  // namespace

WidgetInputHandlerImpl::WidgetInputHandlerImpl(
    scoped_refptr<WidgetInputHandlerManager> manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    scoped_refptr<MainThreadEventQueue> input_event_queue,
    base::WeakPtr<RenderWidget> render_widget)
    : main_thread_task_runner_(main_thread_task_runner),
      input_handler_manager_(manager),
      input_event_queue_(input_event_queue),
      render_widget_(render_widget),
      binding_(this),
      associated_binding_(this) {}

WidgetInputHandlerImpl::~WidgetInputHandlerImpl() {}

void WidgetInputHandlerImpl::SetAssociatedBinding(
    mojom::WidgetInputHandlerAssociatedRequest request) {
  associated_binding_.Bind(std::move(request));
  associated_binding_.set_connection_error_handler(
      base::Bind(&WidgetInputHandlerImpl::Release, base::Unretained(this)));
}

void WidgetInputHandlerImpl::SetBinding(
    mojom::WidgetInputHandlerRequest request) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::Bind(&WidgetInputHandlerImpl::Release, base::Unretained(this)));
}

void WidgetInputHandlerImpl::SetFocus(bool focused) {
  RunOnMainThread(
      base::Bind(&RenderWidget::OnSetFocus, render_widget_, focused));
}

void WidgetInputHandlerImpl::MouseCaptureLost() {
  RunOnMainThread(
      base::Bind(&RenderWidget::OnMouseCaptureLost, render_widget_));
}

void WidgetInputHandlerImpl::SetEditCommandsForNextKeyEvent(
    const std::vector<EditCommand>& commands) {
  RunOnMainThread(base::Bind(&RenderWidget::OnSetEditCommandsForNextKeyEvent,
                             render_widget_, commands));
}

void WidgetInputHandlerImpl::CursorVisibilityChanged(bool visible) {
  RunOnMainThread(base::Bind(&RenderWidget::OnCursorVisibilityChange,
                             render_widget_, visible));
}

void WidgetInputHandlerImpl::ImeSetComposition(
    const base::string16& text,
    const std::vector<ui::CompositionUnderline>& underlines,
    const gfx::Range& range,
    int32_t start,
    int32_t end) {
  RunOnMainThread(base::Bind(
      &RenderWidget::OnImeSetComposition, render_widget_, text,
      ConvertUIUnderlinesToBlinkUnderlines(underlines), range, start, end));
}

void WidgetInputHandlerImpl::ImeCommitText(
    const base::string16& text,
    const std::vector<ui::CompositionUnderline>& underlines,
    const gfx::Range& range,
    int32_t relative_cursor_position) {
  RunOnMainThread(base::Bind(&RenderWidget::OnImeCommitText, render_widget_,
                             text,
                             ConvertUIUnderlinesToBlinkUnderlines(underlines),
                             range, relative_cursor_position));
}

void WidgetInputHandlerImpl::ImeFinishComposingText(bool keep_selection) {
  RunOnMainThread(base::Bind(&RenderWidget::OnImeFinishComposingText,
                             render_widget_, keep_selection));
}

void WidgetInputHandlerImpl::RequestTextInputStateUpdate() {
  RunOnMainThread(
      base::Bind(&RenderWidget::OnRequestTextInputStateUpdate, render_widget_));
}

void WidgetInputHandlerImpl::RequestCompositionUpdates(bool immediate_request,
                                                       bool monitor_request) {
  RunOnMainThread(base::Bind(&RenderWidget::OnRequestCompositionUpdates,
                             render_widget_, immediate_request,
                             monitor_request));
}

void WidgetInputHandlerImpl::DispatchEvent(
    std::unique_ptr<content::InputEvent> event,
    DispatchEventCallback callback) {
  if (!event || !event->web_event) {
    return;
  }
  input_handler_manager_->DispatchEvent(std::move(event), std::move(callback));
}

void WidgetInputHandlerImpl::DispatchNonBlockingEvent(
    std::unique_ptr<content::InputEvent> event) {
  DispatchEvent(std::move(event), DispatchEventCallback());
}

void WidgetInputHandlerImpl::RunOnMainThread(base::OnceClosure closure) {
  if (input_event_queue_) {
    input_event_queue_->QueueClosure(std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void WidgetInputHandlerImpl::Release() {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    // Close the binding on the compositor thread first before telling the main
    // thread to delete this object.
    associated_binding_.Close();
    binding_.Close();
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&WidgetInputHandlerImpl::Release, base::Unretained(this)));
    return;
  }
  delete this;
}

}  // namespace content
