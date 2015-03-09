// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_TOUCH_EDITABLE_IMPL_AURA_H_
#define CONTENT_BROWSER_WEB_CONTENTS_TOUCH_EDITABLE_IMPL_AURA_H_

#include <deque>
#include <map>
#include <queue>

#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "ui/aura/window_observer.h"
#include "ui/base/touch/touch_editing_controller.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class Accelerator;
}

namespace content {
class TouchEditableImplAuraTest;

// Aura specific implementation of ui::TouchEditable for a RenderWidgetHostView.
class CONTENT_EXPORT TouchEditableImplAura
    : public ui::TouchEditable,
      public NON_EXPORTED_BASE(RenderWidgetHostViewAura::TouchEditingClient) {
 public:
  ~TouchEditableImplAura() override;

  static TouchEditableImplAura* Create();

  void AttachToView(RenderWidgetHostViewAura* view);

  // Updates the |touch_selection_controller_| or ends touch editing session
  // depending on the current selection and cursor state.
  void UpdateEditingController();

  void OverscrollStarted();
  void OverscrollCompleted();

  // Overridden from RenderWidgetHostViewAura::TouchEditingClient.
  void StartTouchEditing() override;
  void EndTouchEditing(bool quick) override;
  void OnSelectionOrCursorChanged(const ui::SelectionBound& anchor,
                                  const ui::SelectionBound& focus) override;
  void OnTextInputTypeChanged(ui::TextInputType type) override;
  bool HandleInputEvent(const ui::Event* event) override;
  void GestureEventAck(int gesture_event_type) override;
  void DidStopFlinging() override;
  void OnViewDestroyed() override;

  // Overridden from ui::TouchEditable:
  void SelectRect(const gfx::Point& start, const gfx::Point& end) override;
  void MoveCaretTo(const gfx::Point& point) override;
  void GetSelectionEndPoints(ui::SelectionBound* anchor,
                             ui::SelectionBound* focus) override;
  gfx::Rect GetBounds() override;
  gfx::NativeView GetNativeView() const override;
  void ConvertPointToScreen(gfx::Point* point) override;
  void ConvertPointFromScreen(gfx::Point* point) override;
  bool DrawsHandles() override;
  void OpenContextMenu(const gfx::Point& anchor) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void DestroyTouchSelection() override;

 protected:
  TouchEditableImplAura();

 private:
  friend class TouchEditableImplAuraTest;

  // A convenience function that is called after scroll/fling/overscroll ends to
  // re-activate touch selection if necessary.
  void StartTouchEditingIfNecessary();

  void Cleanup();

  // Bounds for the selection.
  ui::SelectionBound selection_anchor_;
  ui::SelectionBound selection_focus_;

  // The current text input type.
  ui::TextInputType text_input_type_;

  RenderWidgetHostViewAura* rwhva_;
  scoped_ptr<ui::TouchEditingControllerDeprecated> touch_selection_controller_;

  // True if |rwhva_| is currently handling a gesture that could result in a
  // change in selection (long press, double tap or triple tap).
  bool selection_gesture_in_process_;

  // Set to true if handles are hidden when user is scrolling. Used to determine
  // whether to re-show handles after a scrolling session.
  bool handles_hidden_due_to_scroll_;

  // Keep track of scrolls/overscrolls in progress.
  bool scroll_in_progress_;
  bool overscroll_in_progress_;

  // Used to track if a textfield was focused when the current tap gesture
  // happened.
  bool textfield_was_focused_on_tap_;

  DISALLOW_COPY_AND_ASSIGN(TouchEditableImplAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_TOUCH_EDITABLE_IMPL_AURA_H_
