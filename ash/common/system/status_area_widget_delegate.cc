// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/status_area_widget_delegate.h"

#include "ash/ash_export.h"
#include "ash/common/ash_switches.h"
#include "ash/common/focus_cycler.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/border.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

const int kAnimationDurationMs = 250;

class StatusAreaWidgetDelegateAnimationSettings
    : public ui::ScopedLayerAnimationSettings {
 public:
  explicit StatusAreaWidgetDelegateAnimationSettings(ui::Layer* layer)
      : ui::ScopedLayerAnimationSettings(layer->GetAnimator()) {
    SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
    SetPreemptionStrategy(ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    SetTweenType(gfx::Tween::EASE_IN_OUT);
  }

  ~StatusAreaWidgetDelegateAnimationSettings() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidgetDelegateAnimationSettings);
};

}  // namespace

namespace ash {

StatusAreaWidgetDelegate::StatusAreaWidgetDelegate()
    : focus_cycler_for_testing_(nullptr), alignment_(SHELF_ALIGNMENT_BOTTOM) {
  // Allow the launcher to surrender the focus to another window upon
  // navigation completion by the user.
  set_allow_deactivate_on_esc(true);
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
}

StatusAreaWidgetDelegate::~StatusAreaWidgetDelegate() {}

void StatusAreaWidgetDelegate::SetFocusCyclerForTesting(
    const FocusCycler* focus_cycler) {
  focus_cycler_for_testing_ = focus_cycler;
}

views::View* StatusAreaWidgetDelegate::GetDefaultFocusableChild() {
  return child_at(0);
}

views::Widget* StatusAreaWidgetDelegate::GetWidget() {
  return View::GetWidget();
}

const views::Widget* StatusAreaWidgetDelegate::GetWidget() const {
  return View::GetWidget();
}

void StatusAreaWidgetDelegate::OnGestureEvent(ui::GestureEvent* event) {
  views::Widget* target_widget =
      static_cast<views::View*>(event->target())->GetWidget();
  WmWindow* target_window = WmLookup::Get()->GetWindowForWidget(target_widget);
  WmShelf* shelf = target_window->GetRootWindowController()->GetShelf();
  if (shelf->ProcessGestureEvent(*event, target_window))
    event->StopPropagation();
  else
    views::AccessiblePaneView::OnGestureEvent(event);
}

bool StatusAreaWidgetDelegate::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  const FocusCycler* focus_cycler = focus_cycler_for_testing_
                                        ? focus_cycler_for_testing_
                                        : WmShell::Get()->focus_cycler();
  return focus_cycler->widget_activating() == GetWidget();
}

void StatusAreaWidgetDelegate::DeleteDelegate() {}

void StatusAreaWidgetDelegate::AddTray(views::View* tray) {
  SetLayoutManager(NULL);  // Reset layout manager before adding a child.
  AddChildView(tray);
  // Set the layout manager with the new list of children.
  UpdateLayout();
}

void StatusAreaWidgetDelegate::UpdateLayout() {
  // Use a grid layout so that the trays can be centered in each cell, and
  // so that the widget gets laid out correctly when tray sizes change.
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Update tray border based on layout.
  bool is_child_on_edge = true;
  for (int c = 0; c < child_count(); ++c) {
    views::View* child = child_at(c);
    if (!child->visible())
      continue;
    SetBorderOnChild(child, is_child_on_edge);
    is_child_on_edge = false;
  }

  views::ColumnSet* columns = layout->AddColumnSet(0);

  if (IsHorizontalAlignment(alignment_)) {
    bool is_first_visible_child = true;
    for (int c = child_count() - 1; c >= 0; --c) {
      views::View* child = child_at(c);
      if (!child->visible())
        continue;
      if (!is_first_visible_child)
        columns->AddPaddingColumn(0, GetTrayConstant(TRAY_SPACING));
      is_first_visible_child = false;
      columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::FILL,
                         0, /* resize percent */
                         views::GridLayout::USE_PREF, 0, 0);
    }
    layout->StartRow(0, 0);
    for (int c = child_count() - 1; c >= 0; --c) {
      views::View* child = child_at(c);
      if (child->visible())
        layout->AddView(child);
    }
  } else {
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                       0, /* resize percent */
                       views::GridLayout::USE_PREF, 0, 0);
    bool is_first_visible_child = true;
    for (int c = child_count() - 1; c >= 0; --c) {
      views::View* child = child_at(c);
      if (!child->visible())
        continue;
      if (!is_first_visible_child)
        layout->AddPaddingRow(0, GetTrayConstant(TRAY_SPACING));
      is_first_visible_child = false;
      layout->StartRow(0, 0);
      layout->AddView(child);
    }
  }

  layer()->GetAnimator()->StopAnimating();
  StatusAreaWidgetDelegateAnimationSettings settings(layer());

  Layout();
  UpdateWidgetSize();
}

void StatusAreaWidgetDelegate::ChildPreferredSizeChanged(View* child) {
  // Need to resize the window when trays or items are added/removed.
  StatusAreaWidgetDelegateAnimationSettings settings(layer());
  UpdateWidgetSize();
}

void StatusAreaWidgetDelegate::ChildVisibilityChanged(View* child) {
  UpdateLayout();
}

void StatusAreaWidgetDelegate::UpdateWidgetSize() {
  if (GetWidget())
    GetWidget()->SetSize(GetPreferredSize());
}

void StatusAreaWidgetDelegate::SetBorderOnChild(views::View* child,
                                                bool extend_border_to_edge) {
  const int shelf_size = GetShelfConstant(SHELF_SIZE);
  const int item_height = GetTrayConstant(TRAY_ITEM_HEIGHT_LEGACY);
  int top_edge, left_edge, bottom_edge, right_edge;

  // Tray views are laid out right-to-left or bottom-to-top.
  if (MaterialDesignController::IsShelfMaterial()) {
    const bool horizontal_alignment = IsHorizontalAlignment(alignment_);
    const int padding = (shelf_size - item_height) / 2;
    const int extended_padding =
        GetTrayConstant(TRAY_PADDING_FROM_EDGE_OF_SHELF);

    top_edge = horizontal_alignment ? padding : 0;
    left_edge = horizontal_alignment ? 0 : padding;
    bottom_edge = horizontal_alignment
                      ? padding
                      : (extend_border_to_edge ? extended_padding : 0);
    right_edge = horizontal_alignment
                     ? (extend_border_to_edge ? extended_padding : 0)
                     : padding;
  } else {
    bool on_edge = (child == child_at(0));
    if (IsHorizontalAlignment(alignment_)) {
      top_edge = kShelfItemInset;
      left_edge = 0;
      bottom_edge = shelf_size - kShelfItemInset - item_height;
      right_edge =
          on_edge ? GetTrayConstant(TRAY_PADDING_FROM_EDGE_OF_SHELF) : 0;
    } else if (alignment_ == SHELF_ALIGNMENT_LEFT) {
      top_edge = 0;
      left_edge = shelf_size - kShelfItemInset - item_height;
      bottom_edge =
          on_edge ? GetTrayConstant(TRAY_PADDING_FROM_EDGE_OF_SHELF) : 0;
      right_edge = kShelfItemInset;
    } else {  // SHELF_ALIGNMENT_RIGHT
      top_edge = 0;
      left_edge = kShelfItemInset;
      bottom_edge =
          on_edge ? GetTrayConstant(TRAY_PADDING_FROM_EDGE_OF_SHELF) : 0;
      right_edge = shelf_size - kShelfItemInset - item_height;
    }
  }
  child->SetBorder(views::Border::CreateEmptyBorder(top_edge, left_edge,
                                                    bottom_edge, right_edge));
  // Layout on |child| needs to be updated based on new border value before
  // displaying; otherwise |child| will be showing with old border size.
  // Fix for crbug.com/623438.
  child->Layout();
}

}  // namespace ash
