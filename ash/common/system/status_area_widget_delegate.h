// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_
#define ASH_COMMON_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
class FocusCycler;

// The View for the status area widget.
class ASH_EXPORT StatusAreaWidgetDelegate : public views::AccessiblePaneView,
                                            public views::WidgetDelegate {
 public:
  StatusAreaWidgetDelegate();
  ~StatusAreaWidgetDelegate() override;

  // Add a tray view to the widget (e.g. system tray, web notifications).
  void AddTray(views::View* tray);

  // Called whenever layout might change (e.g. alignment changed).
  void UpdateLayout();

  // Sets the focus cycler.
  void SetFocusCyclerForTesting(const FocusCycler* focus_cycler);

  void set_alignment(ShelfAlignment alignment) { alignment_ = alignment; }

  // Overridden from views::AccessiblePaneView.
  View* GetDefaultFocusableChild() override;

  // Overridden from views::View:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // Overridden from ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::WidgetDelegate overrides:
  bool CanActivate() const override;
  void DeleteDelegate() override;

 protected:
  // Overridden from views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

 private:
  void UpdateWidgetSize();

  // Sets a border on |child|. If |extend_border_to_edge| is true, then an extra
  // wide border is added to extend the view's hit region to the edge of the
  // screen.
  void SetBorderOnChild(views::View* child, bool extend_border_to_edge);

  const FocusCycler* focus_cycler_for_testing_;

  // TODO(jamescook): Get this from WmShelf.
  ShelfAlignment alignment_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidgetDelegate);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_
