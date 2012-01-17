// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list.h"

#include "ash/app_list/app_list_model.h"
#include "ash/app_list/app_list_view.h"
#include "ash/ash_switches.h"
#include "ash/shell_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

namespace {

// Gets preferred bounds of app list window in show/hide state.
gfx::Rect GetPreferredBounds(bool show) {
  // The y-axis offset used at the beginning of showing animation.
  static const int kMoveUpAnimationOffset = 50;

  gfx::Point cursor = gfx::Screen::GetCursorScreenPoint();
  gfx::Rect work_area = gfx::Screen::GetMonitorWorkAreaNearestPoint(cursor);
  gfx::Rect widget_bounds(work_area);
  widget_bounds.Inset(100, 100);
  if (!show)
    widget_bounds.Offset(0, kMoveUpAnimationOffset);

  return widget_bounds;
}

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppList, public:

AppList::AppList()
    : aura::EventFilter(NULL),
      is_visible_(false),
      widget_(NULL) {
}

AppList::~AppList() {
  ResetWidget();
}

void AppList::SetVisible(bool visible) {
  if (visible == is_visible_)
    return;

  is_visible_ = visible;

  if (widget_) {
    ScheduleAnimation();
  } else if (is_visible_) {
    scoped_ptr<AppListModel> model(new AppListModel);
    Shell::GetInstance()->delegate()->BuildAppListModel(model.get());

    // AppListModel and AppListViewDelegate are owned by AppListView. They
    // will be released with AppListView on close.
    AppListView* app_list_view = new AppListView(
        model.release(),
        Shell::GetInstance()->delegate()->CreateAppListViewDelegate(),
        GetPreferredBounds(false));
    SetWidget(app_list_view->GetWidget());
  }
}

bool AppList::IsVisible() {
  return widget_ && widget_->IsVisible();
}

////////////////////////////////////////////////////////////////////////////////
// AppList, private:

void AppList::SetWidget(views::Widget* widget) {
  DCHECK(widget_ == NULL);

  if (is_visible_) {
    widget_ = widget;
    widget_->AddObserver(this);
    GetLayer(widget_)->GetAnimator()->AddObserver(this);
    Shell::GetInstance()->AddRootWindowEventFilter(this);

    widget_->SetBounds(GetPreferredBounds(false));
    widget_->SetOpacity(0);
    ScheduleAnimation();

    widget_->Show();
  } else {
    widget->Close();
  }
}

void AppList::ResetWidget() {
  if (!widget_)
    return;

  widget_->RemoveObserver(this);
  GetLayer(widget_)->GetAnimator()->RemoveObserver(this);
  Shell::GetInstance()->RemoveRootWindowEventFilter(this);
  widget_ = NULL;
}

void AppList::ScheduleAnimation() {
  ui::Layer* layer = GetLayer(widget_);
  ui::LayerAnimator::ScopedSettings app_list_animation(layer->GetAnimator());
  layer->SetBounds(GetPreferredBounds(is_visible_));
  layer->SetOpacity(is_visible_ ? 1.0 : 0.0);

  ui::Layer* default_container_layer = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_DefaultContainer)->layer();
  ui::LayerAnimator::ScopedSettings default_container_animation(
      default_container_layer->GetAnimator());
  default_container_layer->SetOpacity(is_visible_ ? 0.0 : 1.0);
}

////////////////////////////////////////////////////////////////////////////////
// AppList, aura::EventFilter implementation:

bool AppList::PreHandleKeyEvent(aura::Window* target,
                                aura::KeyEvent* event) {
  return false;
}

bool AppList::PreHandleMouseEvent(aura::Window* target,
                                 aura::MouseEvent* event) {
  if (widget_ && is_visible_ && event->type() == ui::ET_MOUSE_PRESSED) {
    aura::MouseEvent translated(*event, target, widget_->GetNativeView());
    if (!widget_->GetNativeView()->ContainsPoint(translated.location()))
      SetVisible(false);
  }
  return false;
}

ui::TouchStatus AppList::PreHandleTouchEvent(aura::Window* target,
                                             aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus AppList::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
// AppList, ui::LayerAnimationObserver implementation:

void AppList::OnLayerAnimationEnded(
    const ui::LayerAnimationSequence* sequence) {
  if (!is_visible_ )
    widget_->Close();
}

void AppList::OnLayerAnimationAborted(
    const ui::LayerAnimationSequence* sequence) {
}

void AppList::OnLayerAnimationScheduled(
    const ui::LayerAnimationSequence* sequence) {
}

////////////////////////////////////////////////////////////////////////////////
// AppList, views::Widget::Observer implementation:

void AppList::OnWidgetClosing(views::Widget* widget) {
  DCHECK(widget_ == widget);
  if (is_visible_)
    SetVisible(false);
  ResetWidget();
}

void AppList::OnWidgetActivationChanged(views::Widget* widget, bool active) {
  DCHECK(widget_ == widget);
  if (widget_ && is_visible_ && !active)
    SetVisible(false);
}

}  // namespace internal
}  // namespace ash
