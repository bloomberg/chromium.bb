// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/status_area_widget_delegate.h"

#include "ash/common/focus_cycler.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/root_window_controller.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/border.h"
#include "ui/views/layout/grid_layout.h"

namespace {

constexpr int kAnimationDurationMs = 250;

constexpr int kPaddingFromEdgeOfShelf = 3;

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
  SetPaintToLayer();
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
  if (shelf->ProcessGestureEvent(*event))
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
    for (int c = child_count() - 1; c >= 0; --c) {
      views::View* child = child_at(c);
      if (!child->visible())
        continue;
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
    for (int c = child_count() - 1; c >= 0; --c) {
      views::View* child = child_at(c);
      if (!child->visible())
        continue;
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
  // Tray views are laid out right-to-left or bottom-to-top.
  const bool horizontal_alignment = IsHorizontalAlignment(alignment_);
  const int padding = (GetShelfConstant(SHELF_SIZE) - kTrayItemSize) / 2;

  const int top_edge = horizontal_alignment ? padding : 0;
  const int left_edge = horizontal_alignment ? 0 : padding;
  const int bottom_edge =
      horizontal_alignment
          ? padding
          : (extend_border_to_edge ? kPaddingFromEdgeOfShelf : 0);
  const int right_edge =
      horizontal_alignment
          ? (extend_border_to_edge ? kPaddingFromEdgeOfShelf : 0)
          : padding;
  child->SetBorder(
      views::CreateEmptyBorder(top_edge, left_edge, bottom_edge, right_edge));

  // Layout on |child| needs to be updated based on new border value before
  // displaying; otherwise |child| will be showing with old border size.
  // Fix for crbug.com/623438.
  child->Layout();
}

}  // namespace ash
