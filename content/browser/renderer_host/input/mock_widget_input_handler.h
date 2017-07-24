// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_WIDGET_INPUT_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_WIDGET_INPUT_HANDLER_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "content/common/input/input_handler.mojom.h"

namespace content {

class MockWidgetInputHandler : public mojom::WidgetInputHandler {
 public:
  MockWidgetInputHandler();
  ~MockWidgetInputHandler() override;

  class DispatchedEvent {
   public:
    DispatchedEvent(DispatchedEvent&& other);

    DispatchedEvent(std::unique_ptr<content::InputEvent> event,
                    DispatchEventCallback callback);
    ~DispatchedEvent();
    std::unique_ptr<content::InputEvent> event_;
    DispatchEventCallback callback_;
  };

  // mojom::WidgetInputHandler override.
  void SetFocus(bool focused) override;
  void MouseCaptureLost() override;
  void SetEditCommandsForNextKeyEvent(
      const std::vector<content::EditCommand>& commands) override;
  void CursorVisibilityChanged(bool visible) override;
  void ImeSetComposition(
      const base::string16& text,
      const std::vector<ui::CompositionUnderline>& underlines,
      const gfx::Range& range,
      int32_t start,
      int32_t end) override;
  void ImeCommitText(const base::string16& text,
                     const std::vector<ui::CompositionUnderline>& underlines,
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

  std::vector<DispatchedEvent> GetAndResetDispatchedEvents();

 private:
  std::vector<DispatchedEvent> dispatched_events_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ACK_HANDLER_H_
