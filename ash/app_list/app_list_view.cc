// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_view.h"

#include "ash/app_list/app_list_item_view.h"
#include "ash/app_list/app_list_model.h"
#include "ash/app_list/app_list_model_view.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// 0.2 black
const SkColor kBackgroundColor = SkColorSetARGB(0x33, 0, 0, 0);

const float kModelViewAnimationScaleFactor = 0.9f;

ui::Transform GetScaleTransform(AppListModelView* model_view) {
  gfx::Rect pixel_bounds = model_view->GetLayerBoundsInPixel();
  gfx::Point center(pixel_bounds.width() / 2, pixel_bounds.height() / 2);
  return ui::GetScaleTransform(center, kModelViewAnimationScaleFactor);
}

}  // namespace

AppListView::AppListView(
    AppListViewDelegate* delegate,
    const gfx::Rect& bounds)
    : delegate_(delegate),
      model_view_(NULL) {
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));
  Init(bounds);
}

AppListView::~AppListView() {
  // Model is going away, so set to NULL before superclass deletes child views.
  if (model_view_)
    model_view_->SetModel(NULL);
}

void AppListView::AnimateShow(int duration_ms) {
  ui::Layer* layer = model_view_->layer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(duration_ms));
  animation.SetTweenType(ui::Tween::EASE_OUT);
  model_view_->SetTransform(ui::Transform());
}

void AppListView::AnimateHide(int duration_ms) {
  ui::Layer* layer = model_view_->layer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(duration_ms));
  animation.SetTweenType(ui::Tween::EASE_IN);
  model_view_->SetTransform(GetScaleTransform(model_view_));
}

void AppListView::Close() {
  if (GetWidget()->IsVisible())
    Shell::GetInstance()->ToggleAppList();
}

void AppListView::Init(const gfx::Rect& bounds) {
  model_view_ = new AppListModelView(this);
  model_view_->SetPaintToLayer(true);
  model_view_->layer()->SetFillsBoundsOpaquely(false);
  AddChildView(model_view_);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.delegate = this;
  widget_params.keep_on_top = true;
  widget_params.transparent = true;
  widget_params.parent = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_AppListContainer);

  views::Widget* widget = new views::Widget;
  widget->Init(widget_params);
  widget->SetContentsView(this);
  widget->SetBounds(bounds);

  // Turns off default animation.
  widget->SetVisibilityChangedAnimationsEnabled(false);

  // Sets initial transform. AnimateShow changes it back to identity transform.
  model_view_->SetTransform(GetScaleTransform(model_view_));
  UpdateModel();
}

void AppListView::UpdateModel() {
  if (delegate_.get()) {
    scoped_ptr<AppListModel> new_model(new AppListModel);
    delegate_->SetModel(new_model.get());
    delegate_->UpdateModel(std::string());
    model_view_->SetModel(new_model.get());
    model_.reset(new_model.release());
  }
}

views::View* AppListView::GetInitiallyFocusedView() {
  return model_view_;
}

void AppListView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  // Gets work area rect, which is in screen coordinates.
  gfx::Rect workarea = Shell::GetInstance()->shelf()->IsVisible() ?
      ScreenAsh::GetUnmaximizedWorkAreaBounds(GetWidget()->GetNativeView()) :
      gfx::Screen::GetMonitorNearestWindow(
          GetWidget()->GetNativeView()).work_area();

  // Converts |workarea| into view's coordinates.
  gfx::Point origin(workarea.origin());
  views::View::ConvertPointFromScreen(this, &origin);
  workarea.Offset(-origin.x(), -origin.y());

  rect = rect.Intersect(workarea);
  model_view_->SetBoundsRect(rect);
}

bool AppListView::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_ESCAPE) {
    Close();
    return true;
  }

  return false;
}

bool AppListView::OnMousePressed(const views::MouseEvent& event) {
  // If mouse click reaches us, this means user clicks on blank area. So close.
  Close();

  return true;
}

void AppListView::ButtonPressed(views::Button* sender,
                                const views::Event& event) {
  if (sender->GetClassName() != AppListItemView::kViewClassName)
    return;

  if (delegate_.get()) {
    delegate_->OnAppListItemActivated(
        static_cast<AppListItemView*>(sender)->model(),
        event.flags());
  }
  Close();
}

}  // namespace ash
