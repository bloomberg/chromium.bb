// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_ACTIONABLE_VIEW_H_
#define ASH_COMMON_SYSTEM_TRAY_ACTIONABLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/common/system/tray/tray_popup_ink_drop_style.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/custom_button.h"

namespace ash {
class SystemTrayItem;

// A focusable view that performs an action when user clicks on it, or presses
// enter or space when focused. Note that the action is triggered on mouse-up,
// instead of on mouse-down. So if user presses the mouse on the view, then
// moves the mouse out of the view and then releases, then the action will not
// be performed.
// Exported for SystemTray.
//
// TODO(bruthig): Consider removing ActionableView and make clients use
// CustomButtons instead. (See crbug.com/614453)
class ASH_EXPORT ActionableView : public views::ButtonListener,
                                  public views::CustomButton {
 public:
  static const char kViewClassName[];

  // The owner is used to close the system tray bubble. Can be null if
  // the action will not close the bubble.
  ActionableView(SystemTrayItem* owner, TrayPopupInkDropStyle ink_drop_style);

  ~ActionableView() override;

  void SetAccessibleName(const base::string16& name);
  const base::string16& accessible_name() const { return accessible_name_; }

  // Closes the system tray bubble. The |owner_| must not be nullptr.
  void CloseSystemBubble();

 protected:
  SystemTrayItem* owner() { return owner_; }

  // Draws focus rectangle on the canvas.
  // Default implementation draws the focus rectangle with certain inset and
  // color. Subclasses can override to change the default settings.
  virtual void OnPaintFocus(gfx::Canvas* canvas);

  // Returns the bounds to paint the focus rectangle in.
  virtual gfx::Rect GetFocusBounds();

  // Performs an action when user clicks on the view (on mouse-press event), or
  // presses a key when this view is in focus. Returns true if the event has
  // been handled and an action was performed. Returns false otherwise.
  virtual bool PerformAction(const ui::Event& event) = 0;

  // Called after PerformAction() to act upon its result, including showing
  // appropriate ink drop ripple. This will not get called if the view is
  // destroyed during PerformAction(). Default implementation shows triggered
  // ripple if action is performed or hides existing ripple if no action is
  // performed. Subclasses can override to change the default behavior.
  virtual void HandlePerformActionResult(bool action_performed,
                                         const ui::Event& event);

  // Overridden from views::CustomButton.
  const char* GetClassName() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;

  // Overridden from views::ButtonListener.
  void ButtonPressed(Button* sender, const ui::Event& event) override;

 private:
  // Used by ButtonPressed() to determine whether |this| has been destroyed as a
  // result of performing the associated action. This is necessary because in
  // the not-destroyed case ButtonPressed() uses member variables.
  bool* destroyed_;

  SystemTrayItem* owner_;

  base::string16 accessible_name_;

  // Defines the flavor of ink drop ripple/highlight that should be constructed.
  TrayPopupInkDropStyle ink_drop_style_;

  DISALLOW_COPY_AND_ASSIGN(ActionableView);
};

// An ActionableView that can be used with a ButtonListener instead of having to
// extend ActionableView and override PerformAction().
class ASH_EXPORT ButtonListenerActionableView : public ActionableView {
 public:
  ButtonListenerActionableView(SystemTrayItem* owner,
                               TrayPopupInkDropStyle ink_drop_style,
                               views::ButtonListener* listener);

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

 private:
  views::ButtonListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(ButtonListenerActionableView);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_ACTIONABLE_VIEW_H_
