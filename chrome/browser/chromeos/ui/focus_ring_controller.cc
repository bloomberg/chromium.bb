// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/focus_ring_controller.h"

#include "ash/wm/window_util.h"
#include "chrome/browser/chromeos/ui/focus_ring_layer.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

FocusRingController::FocusRingController()
    : visible_(false),
      widget_(NULL) {
}

FocusRingController::~FocusRingController() {
  SetVisible(false);
}

void FocusRingController::SetVisible(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;

  if (visible_) {
    views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
    aura::Window* active_window = ash::wm::GetActiveWindow();
    if (active_window)
      SetWidget(views::Widget::GetWidgetForNativeWindow(active_window));
  } else {
    views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
    SetWidget(NULL);
  }
}

void FocusRingController::UpdateFocusRing() {
  views::View* focused_view = NULL;
  if (widget_ && widget_->GetFocusManager())
    focused_view = widget_->GetFocusManager()->GetFocusedView();

  // No focus ring if no focused view or the focused view covers the whole
  // widget content area (such as RenderWidgetHostWidgetAura).
  if (!focused_view ||
      focused_view->ConvertRectToWidget(focused_view->bounds()) ==
          widget_->GetContentsView()->bounds()) {
    focus_ring_layer_.reset();
    return;
  }

  if (!focus_ring_layer_)
    focus_ring_layer_.reset(new FocusRingLayer);

  focus_ring_layer_->SetForView(focused_view);
}

void FocusRingController::SetWidget(views::Widget* widget) {
  if (widget_) {
    widget_->RemoveObserver(this);
    if (widget_->GetFocusManager())
      widget_->GetFocusManager()->RemoveFocusChangeListener(this);
  }

  widget_ = widget;

  if (widget_) {
    widget_->AddObserver(this);
    if (widget_->GetFocusManager())
      widget_->GetFocusManager()->AddFocusChangeListener(this);
  }

  UpdateFocusRing();
}

void FocusRingController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget_, widget);
  SetWidget(NULL);
}

void FocusRingController::OnWidgetBoundsChanged(views::Widget* widget,
                                                const gfx::Rect& new_bounds) {
  DCHECK_EQ(widget_, widget);
  UpdateFocusRing();
}

void FocusRingController::OnNativeFocusChange(gfx::NativeView focused_before,
                                              gfx::NativeView focused_now) {
  views::Widget* widget =
      focused_now ? views::Widget::GetWidgetForNativeWindow(focused_now) : NULL;
  SetWidget(widget);
}

void FocusRingController::OnWillChangeFocus(views::View* focused_before,
                                            views::View* focused_now) {
}

void FocusRingController::OnDidChangeFocus(views::View* focused_before,
                                           views::View* focused_now) {
  DCHECK_EQ(focused_now, widget_->GetFocusManager()->GetFocusedView());
  UpdateFocusRing();
}

}  // namespace chromeos
