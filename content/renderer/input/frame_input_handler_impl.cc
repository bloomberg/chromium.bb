// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/frame_input_handler_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/WebKit/public/web/WebInputMethodController.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

FrameInputHandlerImpl::FrameInputHandlerImpl(
    base::WeakPtr<RenderFrameImpl> render_frame,
    mojom::FrameInputHandlerRequest request)
    : binding_(this),
      render_frame_(render_frame),
      input_event_queue_(render_frame->GetRenderWidget()->GetInputEventQueue()),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this)

{
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
  // If we have created an input event queue move the mojo request over to the
  // compositor thread.
  if (input_event_queue_) {
    // Mojo channel bound on compositor thread.
    RenderThreadImpl::current()->compositor_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FrameInputHandlerImpl::BindNow,
                                  base::Unretained(this), std::move(request)));
  } else {
    // Mojo channel bound on main thread.
    BindNow(std::move(request));
  }
}

FrameInputHandlerImpl::~FrameInputHandlerImpl() {}

// static
void FrameInputHandlerImpl::CreateMojoService(
    base::WeakPtr<RenderFrameImpl> render_frame,
    mojom::FrameInputHandlerRequest request) {
  DCHECK(render_frame);

  // Owns itself. Will be deleted when message pipe is destroyed.
  new FrameInputHandlerImpl(render_frame, std::move(request));
}

void FrameInputHandlerImpl::RunOnMainThread(const base::Closure& closure) {
  if (input_event_queue_) {
    input_event_queue_->QueueClosure(closure);
  } else {
    closure.Run();
  }
}

void FrameInputHandlerImpl::SetCompositionFromExistingText(
    int32_t start,
    int32_t end,
    const std::vector<ui::CompositionUnderline>& ui_underlines) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::Bind(&FrameInputHandlerImpl::SetCompositionFromExistingText,
                   weak_this_, start, end, ui_underlines));
    return;
  }

  if (!render_frame_)
    return;

  ImeEventGuard guard(render_frame_->GetRenderWidget());
  std::vector<blink::WebCompositionUnderline> underlines;
  for (const auto& underline : ui_underlines) {
    blink::WebCompositionUnderline blink_underline(
        underline.start_offset, underline.end_offset, underline.color,
        underline.thick, underline.background_color);
    underlines.push_back(blink_underline);
  }

  render_frame_->GetWebFrame()->SetCompositionFromExistingText(start, end,
                                                               underlines);
}

void FrameInputHandlerImpl::ExtendSelectionAndDelete(int32_t before,
                                                     int32_t after) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(base::Bind(&FrameInputHandlerImpl::ExtendSelectionAndDelete,
                               weak_this_, before, after));
    return;
  }
  if (!render_frame_)
    return;
  render_frame_->GetWebFrame()->ExtendSelectionAndDelete(before, after);
}

void FrameInputHandlerImpl::DeleteSurroundingText(int32_t before,
                                                  int32_t after) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(base::Bind(&FrameInputHandlerImpl::DeleteSurroundingText,
                               weak_this_, before, after));
    return;
  }
  if (!render_frame_)
    return;
  render_frame_->GetWebFrame()->DeleteSurroundingText(before, after);
}

void FrameInputHandlerImpl::DeleteSurroundingTextInCodePoints(int32_t before,
                                                              int32_t after) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::Bind(&FrameInputHandlerImpl::DeleteSurroundingTextInCodePoints,
                   weak_this_, before, after));
    return;
  }
  if (!render_frame_)
    return;
  render_frame_->GetWebFrame()->DeleteSurroundingTextInCodePoints(before,
                                                                  after);
}

void FrameInputHandlerImpl::SetEditableSelectionOffsets(int32_t start,
                                                        int32_t end) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::Bind(&FrameInputHandlerImpl::SetEditableSelectionOffsets,
                   weak_this_, start, end));
    return;
  }
  if (!render_frame_)
    return;
  render_frame_->GetWebFrame()->SetEditableSelectionOffsets(start, end);
}

void FrameInputHandlerImpl::ExecuteEditCommand(
    const std::string& command,
    const base::Optional<base::string16>& value) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(base::Bind(&FrameInputHandlerImpl::ExecuteEditCommand,
                               weak_this_, command, value));
    return;
  }
  if (!render_frame_)
    return;
  if (value) {
    render_frame_->GetWebFrame()->ExecuteCommand(
        blink::WebString::FromUTF8(command),
        blink::WebString::FromUTF16(value.value()));
    return;
  }

  render_frame_->GetWebFrame()->ExecuteCommand(
      blink::WebString::FromUTF8(command));
}

void FrameInputHandlerImpl::Undo() {
  RunOnMainThread(base::Bind(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                             weak_this_, "Undo", UpdateState::kNone));
}

void FrameInputHandlerImpl::Redo() {
  RunOnMainThread(base::Bind(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                             weak_this_, "Redo", UpdateState::kNone));
}

void FrameInputHandlerImpl::Cut() {
  RunOnMainThread(base::Bind(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                             weak_this_, "Cut",
                             UpdateState::kIsSelectingRange));
}

void FrameInputHandlerImpl::Copy() {
  RunOnMainThread(base::Bind(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                             weak_this_, "Copy",
                             UpdateState::kIsSelectingRange));
}

void FrameInputHandlerImpl::CopyToFindPboard() {
#if defined(OS_MACOSX)
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::Bind(&FrameInputHandlerImpl::CopyToFindPboard, weak_this_));
    return;
  }
  if (!render_frame_)
    return;
  render_frame_->OnCopyToFindPboard();
#endif
}

void FrameInputHandlerImpl::Paste() {
  RunOnMainThread(base::Bind(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                             weak_this_, "Paste", UpdateState::kIsPasting));
}

void FrameInputHandlerImpl::PasteAndMatchStyle() {
  RunOnMainThread(base::Bind(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                             weak_this_, "PasteAndMatchStyle",
                             UpdateState::kIsPasting));
}

void FrameInputHandlerImpl::Replace(const base::string16& word) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::Bind(&FrameInputHandlerImpl::Replace, weak_this_, word));
    return;
  }
  if (!render_frame_)
    return;
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  if (frame->HasSelection())
    frame->SelectWordAroundCaret();
  frame->ReplaceSelection(blink::WebString::FromUTF16(word));
  render_frame_->SyncSelectionIfRequired();
}

void FrameInputHandlerImpl::ReplaceMisspelling(const base::string16& word) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(base::Bind(&FrameInputHandlerImpl::ReplaceMisspelling,
                               weak_this_, word));
    return;
  }
  if (!render_frame_)
    return;
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  if (!frame->HasSelection())
    return;
  frame->ReplaceMisspelledRange(blink::WebString::FromUTF16(word));
}

void FrameInputHandlerImpl::Delete() {
  RunOnMainThread(base::Bind(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                             weak_this_, "Delete", UpdateState::kNone));
}

void FrameInputHandlerImpl::SelectAll() {
  RunOnMainThread(base::Bind(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                             weak_this_, "SelectAll",
                             UpdateState::kIsSelectingRange));
}

void FrameInputHandlerImpl::CollapseSelection() {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::Bind(&FrameInputHandlerImpl::CollapseSelection, weak_this_));
    return;
  }

  if (!render_frame_)
    return;
  const blink::WebRange& range = render_frame_->GetWebFrame()
                                     ->GetInputMethodController()
                                     ->GetSelectionOffsets();
  if (range.IsNull())
    return;

  HandlingState handling_state(render_frame_.get(),
                               UpdateState::kIsSelectingRange);
  render_frame_->GetWebFrame()->SelectRange(
      blink::WebRange(range.EndOffset(), 0));
}

void FrameInputHandlerImpl::SelectRange(const gfx::Point& base,
                                        const gfx::Point& extent) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    // TODO(dtapuska): This event should be coalesced. Chrome IPC uses
    // one outstanding event and an ACK to handle coalescing on the browser
    // side. We should be able to clobber them in the main thread event queue.
    RunOnMainThread(base::Bind(&FrameInputHandlerImpl::SelectRange, weak_this_,
                               base, extent));
    return;
  }

  if (!render_frame_)
    return;
  RenderViewImpl* render_view = render_frame_->render_view();
  HandlingState handling_state(render_frame_.get(),
                               UpdateState::kIsSelectingRange);
  render_frame_->GetWebFrame()->SelectRange(
      render_view->ConvertWindowPointToViewport(base),
      render_view->ConvertWindowPointToViewport(extent));
}

void FrameInputHandlerImpl::AdjustSelectionByCharacterOffset(int32_t start,
                                                             int32_t end) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::Bind(&FrameInputHandlerImpl::AdjustSelectionByCharacterOffset,
                   weak_this_, start, end));
    return;
  }

  if (!render_frame_)
    return;
  blink::WebRange range = render_frame_->GetWebFrame()
                              ->GetInputMethodController()
                              ->GetSelectionOffsets();
  if (range.IsNull())
    return;

  // Sanity checks to disallow empty and out of range selections.
  if (start - end > range.length() || range.StartOffset() + start < 0)
    return;

  HandlingState handling_state(render_frame_.get(),
                               UpdateState::kIsSelectingRange);
  // A negative adjust amount moves the selection towards the beginning of
  // the document, a positive amount moves the selection towards the end of
  // the document.
  render_frame_->GetWebFrame()->SelectRange(
      blink::WebRange(range.StartOffset() + start,
                      range.length() + end - start),
      blink::WebLocalFrame::kPreserveHandleVisibility);
}

void FrameInputHandlerImpl::MoveRangeSelectionExtent(const gfx::Point& extent) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    // TODO(dtapuska): This event should be coalesced. Chrome IPC uses
    // one outstanding event and an ACK to handle coalescing on the browser
    // side. We should be able to clobber them in the main thread event queue.
    RunOnMainThread(base::Bind(&FrameInputHandlerImpl::MoveRangeSelectionExtent,
                               weak_this_, extent));
    return;
  }

  if (!render_frame_)
    return;
  HandlingState handling_state(render_frame_.get(),
                               UpdateState::kIsSelectingRange);
  render_frame_->GetWebFrame()->MoveRangeSelectionExtent(
      render_frame_->render_view()->ConvertWindowPointToViewport(extent));
}

void FrameInputHandlerImpl::ScrollFocusedEditableNodeIntoRect(
    const gfx::Rect& rect) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::Bind(&FrameInputHandlerImpl::ScrollFocusedEditableNodeIntoRect,
                   weak_this_, rect));
    return;
  }

  if (!render_frame_)
    return;

  RenderViewImpl* render_view = render_frame_->render_view();
  render_view->ScrollFocusedEditableNodeIntoRect(rect);
}

void FrameInputHandlerImpl::MoveCaret(const gfx::Point& point) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::Bind(&FrameInputHandlerImpl::MoveCaret, weak_this_, point));
    return;
  }

  if (!render_frame_)
    return;

  RenderViewImpl* render_view = render_frame_->render_view();
  render_frame_->GetWebFrame()->MoveCaretSelection(
      render_view->ConvertWindowPointToViewport(point));
}

void FrameInputHandlerImpl::ExecuteCommandOnMainThread(
    const std::string& command,
    UpdateState update_state) {
  if (!render_frame_)
    return;

  HandlingState handling_state(render_frame_.get(), update_state);
  render_frame_->GetWebFrame()->ExecuteCommand(
      blink::WebString::FromUTF8(command));
}

void FrameInputHandlerImpl::Release() {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    // Close the binding on the compositor thread first before telling the main
    // thread to delete this object.
    binding_.Close();
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&FrameInputHandlerImpl::Release, weak_this_));
    return;
  }
  delete this;
}

void FrameInputHandlerImpl::BindNow(mojom::FrameInputHandlerRequest request) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::Bind(&FrameInputHandlerImpl::Release, base::Unretained(this)));
}

FrameInputHandlerImpl::HandlingState::HandlingState(
    RenderFrameImpl* render_frame,
    UpdateState state)
    : render_frame_(render_frame),
      original_select_range_value_(render_frame->handling_select_range()),
      original_pasting_value_(render_frame->IsPasting()) {
  switch (state) {
    case UpdateState::kIsPasting:
      render_frame->set_is_pasting(true);
    case UpdateState::kIsSelectingRange:
      render_frame->set_handling_select_range(true);
      break;
    case UpdateState::kNone:
      break;
  }
}

FrameInputHandlerImpl::HandlingState::~HandlingState() {
  render_frame_->set_handling_select_range(original_select_range_value_);
  render_frame_->set_is_pasting(original_pasting_value_);
}

}  // namespace content
