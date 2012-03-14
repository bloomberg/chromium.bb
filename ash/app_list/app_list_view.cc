// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_view.h"

#include "ash/app_list/app_list_item_view.h"
#include "ash/app_list/app_list_model.h"
#include "ash/app_list/app_list_model_view.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/shell.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Margins in pixels from work area edges.
const int kLeftRightMargin = 45;
const int kTopBottomMargin = 32;

// 0.2 black
const SkColor kBackgroundColor = SkColorSetARGB(0x33, 0, 0, 0);

const float kModelViewAnimationScaleFactor = 0.9f;

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
}

void AppListView::AnimateShow() {
  ui::Layer* layer = model_view_->layer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  model_view_->SetTransform(ui::Transform());
}

void AppListView::AnimateHide() {
  ui::Layer* layer = model_view_->layer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  model_view_->SetTransform(
      ui::GetScaleTransform(gfx::Point(model_view_->width() / 2,
                                       model_view_->height() / 2),
                            kModelViewAnimationScaleFactor));
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

  views::Widget* widget = new views::Widget;
  widget->Init(widget_params);
  widget->SetContentsView(this);
  widget->SetBounds(bounds);

  // Turns off default animation.
  widget->SetVisibilityChangedAnimationsEnabled(false);

  // Sets initial transform. AnimateShow changes it back to identity transform.
  model_view_->SetTransform(
      ui::GetScaleTransform(gfx::Point(model_view_->width() / 2,
                                       model_view_->height() / 2),
                            kModelViewAnimationScaleFactor));

  UpdateModel();
}

void AppListView::UpdateModel() {
  if (delegate_.get()) {
    scoped_ptr<AppListModel> new_model(new AppListModel);
    delegate_->BuildAppListModel(std::string(), new_model.get());
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
  gfx::Rect workarea = gfx::Screen::GetMonitorWorkAreaNearestWindow(
      GetWidget()->GetNativeView());

  // Converts |workarea| into view's coordinates.
  gfx::Point origin(workarea.origin());
  views::View::ConvertPointFromScreen(this, &origin);
  workarea.Offset(-origin.x(), -origin.y());

  rect = rect.Intersect(workarea);
  rect.Inset(kLeftRightMargin, kTopBottomMargin);
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
