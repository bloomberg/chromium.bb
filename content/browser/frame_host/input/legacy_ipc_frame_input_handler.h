// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_INPUT_LEGACY_IPC_FRAME_INPUT_HANDLER_H_
#define CONTENT_BROWSER_FRAME_HOST_INPUT_LEGACY_IPC_FRAME_INPUT_HANDLER_H_

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/input/input_handler.mojom.h"

namespace content {

// An instance of a mojom::FrameInputHandler based on chrome IPC.
// This class is a temporary class to allow the input messages to
// remain as Chrome IPC messages but progressively work at moving
// them to mojo.
class CONTENT_EXPORT LegacyIPCFrameInputHandler
    : public mojom::FrameInputHandler {
 public:
  explicit LegacyIPCFrameInputHandler(RenderFrameHostImpl* frame_host);
  ~LegacyIPCFrameInputHandler() override;

  // mojom::FrameInputHandler overrides.
  void SetCompositionFromExistingText(
      int32_t start,
      int32_t end,
      const std::vector<ui::ImeTextSpan>& ime_text_spans) override;
  void ExtendSelectionAndDelete(int32_t before, int32_t after) override;
  void DeleteSurroundingText(int32_t before, int32_t after) override;
  void DeleteSurroundingTextInCodePoints(int32_t before,
                                         int32_t after) override;
  void SetEditableSelectionOffsets(int32_t start, int32_t end) override;
  void ExecuteEditCommand(const std::string& command,
                          const base::Optional<base::string16>& value) override;
  void Undo() override;
  void Redo() override;
  void Cut() override;
  void Copy() override;
  void CopyToFindPboard() override;
  void Paste() override;
  void PasteAndMatchStyle() override;
  void Replace(const base::string16& word) override;
  void ReplaceMisspelling(const base::string16& word) override;
  void Delete() override;
  void SelectAll() override;
  void CollapseSelection() override;
  void SelectRange(const gfx::Point& base, const gfx::Point& extent) override;
  void AdjustSelectionByCharacterOffset(
      int32_t start,
      int32_t end,
      blink::mojom::SelectionMenuBehavior selection_menu_behavior) override;
  void MoveRangeSelectionExtent(const gfx::Point& extent) override;
  void ScrollFocusedEditableNodeIntoRect(const gfx::Rect& rect) override;
  void MoveCaret(const gfx::Point& point) override;
  void GetWidgetInputHandler(
      mojom::WidgetInputHandlerAssociatedRequest interface_request,
      mojom::WidgetInputHandlerHostPtr host) override;

 private:
  void SendInput(std::unique_ptr<IPC::Message> message);

  RenderFrameHostImpl* frame_host_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(LegacyIPCFrameInputHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_INPUT_LEGACY_IPC_FRAME_INPUT_HANDLER_H_
