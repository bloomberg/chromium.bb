// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_item_view.h"

#include "ui/base/animation/slide_animation.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {
const int kTrayIconHeight = 29;
const int kTrayItemAnimationDurationMS = 200;
}

namespace ash {
namespace internal {

TrayItemView::TrayItemView()
    : label_(NULL),
      image_view_(NULL) {
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(false);
  SetLayoutManager(new views::FillLayout);
}

TrayItemView::~TrayItemView() {}

void TrayItemView::CreateLabel() {
  label_ = new views::Label;
  AddChildView(label_);
}

void TrayItemView::CreateImageView() {
  image_view_ = new views::ImageView;
  AddChildView(image_view_);
}

void TrayItemView::SetVisible(bool set_visible) {
  if (!GetWidget()) {
    views::View::SetVisible(set_visible);
    return;
  }

  if (!animation_.get()) {
    animation_.reset(new ui::SlideAnimation(this));
    animation_->SetSlideDuration(GetAnimationDurationMS());
    animation_->SetTweenType(ui::Tween::LINEAR);
    animation_->Reset(visible() ? 1.0 : 0.0);
  }

  if (!set_visible) {
    animation_->Hide();
    AnimationProgressed(animation_.get());
  } else {
    animation_->Show();
    AnimationProgressed(animation_.get());
    views::View::SetVisible(true);
  }
}

gfx::Size TrayItemView::DesiredSize() {
  return views::View::GetPreferredSize();
}

int TrayItemView::GetAnimationDurationMS() {
  return kTrayItemAnimationDurationMS;
}

gfx::Size TrayItemView::GetPreferredSize() {
  gfx::Size size = DesiredSize();
  size.set_height(kTrayIconHeight);
  if (!animation_.get() || !animation_->is_animating())
    return size;
  size.set_width(std::max(1,
      static_cast<int>(size.width() * animation_->GetCurrentValue())));
  return size;
}

void TrayItemView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void TrayItemView::AnimationProgressed(const ui::Animation* animation) {
  ui::Transform transform;
  transform.SetScale(animation->GetCurrentValue(),
                     animation->GetCurrentValue());
  transform.ConcatTranslate(0, animation->CurrentValueBetween(
      static_cast<double>(height()) / 2, 0.));
  layer()->SetTransform(transform);
  PreferredSizeChanged();
}

void TrayItemView::AnimationEnded(const ui::Animation* animation) {
  if (animation->GetCurrentValue() < 0.1)
    views::View::SetVisible(false);
}

void TrayItemView::AnimationCanceled(const ui::Animation* animation) {
  AnimationEnded(animation);
}

}  // namespace internal
}  // namespace ash
