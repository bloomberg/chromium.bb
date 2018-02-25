// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_LEGACY_IPC_WIDGET_INPUT_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_LEGACY_IPC_WIDGET_INPUT_HANDLER_H_

#include "content/common/input/input_handler.mojom.h"

namespace content {

class LegacyInputRouterImpl;

// An instance of a mojom::WidgetInputHandler based on chrome IPC.
// This class is a temporary class to allow the input messages to
// remain as Chrome IPC messages but progressively work at moving
// them to mojo.
class CONTENT_EXPORT LegacyIPCWidgetInputHandler
    : public mojom::WidgetInputHandler {
 public:
  explicit LegacyIPCWidgetInputHandler(LegacyInputRouterImpl* input_router);
  ~LegacyIPCWidgetInputHandler() override;

  // mojom::WidgetInputHandler overrides.
  void SetFocus(bool focused) override;
  void MouseCaptureLost() override;
  void SetEditCommandsForNextKeyEvent(
      const std::vector<EditCommand>& commands) override;
  void CursorVisibilityChanged(bool visible) override;
  void ImeSetComposition(const base::string16& text,
                         const std::vector<ui::ImeTextSpan>& ime_text_spans,
                         const gfx::Range& range,
                         int32_t start,
                         int32_t end) override;
  void ImeCommitText(const base::string16& text,
                     const std::vector<ui::ImeTextSpan>& ime_text_spans,
                     const gfx::Range& range,
                     int32_t relative_cursor_position) override;
  void ImeFinishComposingText(bool keep_selection) override;
  void RequestTextInputStateUpdate() override;
  void RequestCompositionUpdates(bool immediate_request,
                                 bool monitor_request) override;
  void DispatchEvent(std::unique_ptr<content::InputEvent> event,
                     DispatchEventCallback callback) override;
  void DispatchNonBlockingEvent(
      std::unique_ptr<content::InputEvent> event) override;
  void AttachSynchronousCompositor(
      mojom::SynchronousCompositorControlHostPtr control_host,
      mojom::SynchronousCompositorHostAssociatedPtrInfo host,
      mojom::SynchronousCompositorAssociatedRequest compositor_request)
      override;

 private:
  void SendInput(std::unique_ptr<IPC::Message> message);

  LegacyInputRouterImpl* input_router_;

  DISALLOW_COPY_AND_ASSIGN(LegacyIPCWidgetInputHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_LEGACY_IPC_WIDGET_INPUT_HANDLER_H_
