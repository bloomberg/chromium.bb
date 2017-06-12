// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/input/legacy_ipc_frame_input_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/input/legacy_input_router_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/input_messages.h"

namespace content {

LegacyIPCFrameInputHandler::LegacyIPCFrameInputHandler(
    RenderFrameHostImpl* frame_host,
    int routing_id)
    : frame_host_(frame_host), routing_id_(routing_id) {
  DCHECK(frame_host);
}

LegacyIPCFrameInputHandler::~LegacyIPCFrameInputHandler() {}

void LegacyIPCFrameInputHandler::SetCompositionFromExistingText(
    int32_t start,
    int32_t end,
    const std::vector<ui::CompositionUnderline>& ui_underlines) {
  std::vector<blink::WebCompositionUnderline> underlines;
  for (const auto& underline : ui_underlines) {
    blink::WebCompositionUnderline blink_underline(
        underline.start_offset, underline.end_offset, underline.color,
        underline.thick, underline.background_color);
    underlines.push_back(blink_underline);
  }

  SendInput(base::MakeUnique<InputMsg_SetCompositionFromExistingText>(
      routing_id_, start, end, underlines));
}

void LegacyIPCFrameInputHandler::ExtendSelectionAndDelete(int32_t before,
                                                          int32_t after) {
  SendInput(base::MakeUnique<InputMsg_ExtendSelectionAndDelete>(routing_id_,
                                                                before, after));
}

void LegacyIPCFrameInputHandler::DeleteSurroundingText(int32_t before,
                                                       int32_t after) {
  SendInput(base::MakeUnique<InputMsg_DeleteSurroundingText>(routing_id_,
                                                             before, after));
}

void LegacyIPCFrameInputHandler::DeleteSurroundingTextInCodePoints(
    int32_t before,
    int32_t after) {
  SendInput(base::MakeUnique<InputMsg_DeleteSurroundingTextInCodePoints>(
      routing_id_, before, after));
}

void LegacyIPCFrameInputHandler::SetEditableSelectionOffsets(int32_t start,
                                                             int32_t end) {
  SendInput(base::MakeUnique<InputMsg_SetEditableSelectionOffsets>(routing_id_,
                                                                   start, end));
}

void LegacyIPCFrameInputHandler::ExecuteEditCommand(
    const std::string& command,
    const base::Optional<base::string16>& value) {
  if (!value) {
    SendInput(base::MakeUnique<InputMsg_ExecuteNoValueEditCommand>(routing_id_,
                                                                   command));
  }
}

void LegacyIPCFrameInputHandler::Undo() {
  SendInput(base::MakeUnique<InputMsg_Undo>(routing_id_));
}

void LegacyIPCFrameInputHandler::Redo() {
  SendInput(base::MakeUnique<InputMsg_Redo>(routing_id_));
}

void LegacyIPCFrameInputHandler::Cut() {
  SendInput(base::MakeUnique<InputMsg_Cut>(routing_id_));
}

void LegacyIPCFrameInputHandler::Copy() {
  SendInput(base::MakeUnique<InputMsg_Copy>(routing_id_));
}

void LegacyIPCFrameInputHandler::CopyToFindPboard() {
#if defined(OS_MACOSX)
  SendInput(base::MakeUnique<InputMsg_CopyToFindPboard>(routing_id_));
#endif
}

void LegacyIPCFrameInputHandler::Paste() {
  SendInput(base::MakeUnique<InputMsg_Paste>(routing_id_));
}

void LegacyIPCFrameInputHandler::PasteAndMatchStyle() {
  SendInput(base::MakeUnique<InputMsg_PasteAndMatchStyle>(routing_id_));
}

void LegacyIPCFrameInputHandler::Replace(const base::string16& word) {
  SendInput(base::MakeUnique<InputMsg_Replace>(routing_id_, word));
}

void LegacyIPCFrameInputHandler::ReplaceMisspelling(
    const base::string16& word) {
  SendInput(base::MakeUnique<InputMsg_ReplaceMisspelling>(routing_id_, word));
}

void LegacyIPCFrameInputHandler::Delete() {
  SendInput(base::MakeUnique<InputMsg_Delete>(routing_id_));
}

void LegacyIPCFrameInputHandler::SelectAll() {
  SendInput(base::MakeUnique<InputMsg_SelectAll>(routing_id_));
}

void LegacyIPCFrameInputHandler::CollapseSelection() {
  SendInput(base::MakeUnique<InputMsg_CollapseSelection>(routing_id_));
}

void LegacyIPCFrameInputHandler::SelectRange(const gfx::Point& point,
                                             const gfx::Point& extent) {
  SendInput(base::MakeUnique<InputMsg_SelectRange>(routing_id_, point, extent));
}

void LegacyIPCFrameInputHandler::AdjustSelectionByCharacterOffset(int32_t start,
                                                                  int32_t end) {
  SendInput(base::MakeUnique<InputMsg_AdjustSelectionByCharacterOffset>(
      routing_id_, start, end));
}

void LegacyIPCFrameInputHandler::MoveRangeSelectionExtent(
    const gfx::Point& extent) {
  SendInput(
      base::MakeUnique<InputMsg_MoveRangeSelectionExtent>(routing_id_, extent));
}

void LegacyIPCFrameInputHandler::SendInput(
    std::unique_ptr<IPC::Message> message) {
  static_cast<LegacyInputRouterImpl*>(
      frame_host_->GetRenderWidgetHost()->input_router())
      ->SendInput(std::move(message));
}

}  // namespace content
