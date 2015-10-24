// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_AURA_H_

#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "ui/touch_selection/touch_selection_controller.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"

namespace content {
class RenderWidgetHostViewAura;

// An implementation of |TouchSelectionControllerClient| to be used in Aura's
// implementation of touch selection for contents.
class CONTENT_EXPORT TouchSelectionControllerClientAura
    : public ui::TouchSelectionControllerClient,
      public ui::TouchSelectionMenuClient {
 public:
  explicit TouchSelectionControllerClientAura(RenderWidgetHostViewAura* rwhva);
  ~TouchSelectionControllerClientAura() override;

  // Called when |rwhva_|'s window is moved, to update the quick menu's
  // position.
  void OnWindowMoved();

  // Called on first touch down/last touch up to hide/show the quick menu.
  void OnTouchDown();
  void OnTouchUp();

  // Called when touch scroll starts/completes to hide/show touch handles and
  // the quick menu.
  void OnScrollStarted();
  void OnScrollCompleted();

 private:
  friend class TestTouchSelectionControllerClientAura;
  class EnvPreTargetHandler;

  bool IsQuickMenuAllowed() const;
  void ShowQuickMenu();
  void UpdateQuickMenu();

  // ui::TouchSelectionControllerClient:
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override;
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  scoped_ptr<ui::TouchHandleDrawable> CreateDrawable() override;

  // ui::TouchSelectionMenuClient:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void RunContextMenu() override;

  // Not owned, non-null for the lifetime of this object.
  RenderWidgetHostViewAura* rwhva_;

  base::Timer quick_menu_timer_;
  bool touch_down_;
  bool scroll_in_progress_;
  bool handle_drag_in_progress_;
  bool insertion_quick_menu_allowed_;

  bool show_quick_menu_immediately_for_test_;

  // A pre-target event handler for aura::Env which deactivates touch selection
  // on mouse and keyboard events.
  scoped_ptr<EnvPreTargetHandler> env_pre_target_handler_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerClientAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_CLIENT_AURA_H_
