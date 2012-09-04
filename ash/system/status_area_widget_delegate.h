// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/wm/gestures/shelf_gesture_handler.h"
#include "ash/wm/shelf_types.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace internal {

class FocusCycler;

class ASH_EXPORT StatusAreaWidgetDelegate : public views::AccessiblePaneView,
                                            public views::WidgetDelegate {
 public:
  StatusAreaWidgetDelegate();
  virtual ~StatusAreaWidgetDelegate();

  // Add a tray view to the widget (e.g. system tray, web notifications).
  void AddTray(views::View* tray);

  // Called whenever layout might change (e.g. alignment changed).
  void UpdateLayout();

  // Sets the focus cycler.
  void SetFocusCyclerForTesting(const FocusCycler* focus_cycler);

  void set_alignment(ShelfAlignment alignment) { alignment_ = alignment; }

  // Overridden from views::AccessiblePaneView.
  virtual View* GetDefaultFocusableChild() OVERRIDE;

  // Overridden from views::View:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual ui::EventResult OnGestureEvent(
      const ui::GestureEvent& event) OVERRIDE;

  // views::WidgetDelegate overrides:
  virtual bool CanActivate() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

 protected:
  // Overridden from views::View:
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;

 private:
  void UpdateWidgetSize();

  const FocusCycler* focus_cycler_for_testing_;
  ShelfAlignment alignment_;

  ShelfGestureHandler gesture_handler_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidgetDelegate);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_
