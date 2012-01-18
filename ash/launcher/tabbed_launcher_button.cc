// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/tabbed_launcher_button.h"

#include <algorithm>

#include "ash/launcher/launcher_button_host.h"
#include "grit/ui_resources.h"
#include "ui/base/animation/multi_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/insets.h"

namespace ash {
namespace internal {

TabbedLauncherButton::AnimationDelegateImpl::AnimationDelegateImpl(
    TabbedLauncherButton* host)
    : host_(host) {
}

TabbedLauncherButton::AnimationDelegateImpl::~AnimationDelegateImpl() {
}

void TabbedLauncherButton::AnimationDelegateImpl::AnimationEnded(
    const ui::Animation* animation) {
  AnimationProgressed(animation);
  // Hide the image when the animation is done. We'll show it again the next
  // time SetImages is invoked.
  host_->show_image_ = false;
}

void TabbedLauncherButton::AnimationDelegateImpl::AnimationProgressed(
    const ui::Animation* animation) {
  if (host_->animation_->current_part_index() == 1)
    host_->SchedulePaint();
}

// static
TabbedLauncherButton::ImageSet* TabbedLauncherButton::bg_image_1_ = NULL;
TabbedLauncherButton::ImageSet* TabbedLauncherButton::bg_image_2_ = NULL;
TabbedLauncherButton::ImageSet* TabbedLauncherButton::bg_image_3_ = NULL;

TabbedLauncherButton::TabbedLauncherButton(views::ButtonListener* listener,
                                           LauncherButtonHost* host)
    : views::ImageButton(listener),
      host_(host),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_delegate_(this)),
      show_image_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(hover_controller_(this)) {
  if (!bg_image_1_) {
    bg_image_1_ = CreateImageSet(IDR_AURA_LAUNCHER_TABBED_BROWSER_1,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_1_PUSHED,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_1_HOT);
    bg_image_2_ = CreateImageSet(IDR_AURA_LAUNCHER_TABBED_BROWSER_2,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_2_PUSHED,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_2_HOT);
    bg_image_3_ = CreateImageSet(IDR_AURA_LAUNCHER_TABBED_BROWSER_3,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_3_PUSHED,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_3_HOT);
  }
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
}

TabbedLauncherButton::~TabbedLauncherButton() {
}

void TabbedLauncherButton::PrepareForImageChange() {
  if (!show_image_ || (animation_.get() && animation_->is_animating()))
    return;

  // Pause for 500ms, then ease out for 200ms.
  ui::MultiAnimation::Parts animation_parts;
  animation_parts.push_back(ui::MultiAnimation::Part(500, ui::Tween::ZERO));
  animation_parts.push_back(ui::MultiAnimation::Part(200, ui::Tween::EASE_OUT));
  animation_.reset(new ui::MultiAnimation(animation_parts));
  animation_->set_continuous(false);
  animation_->set_delegate(&animation_delegate_);
  animation_->Start();
}

void TabbedLauncherButton::SetTabImage(const SkBitmap& image, int count) {
  animation_.reset();
  show_image_ = true;
  image_ = image;
  ImageSet* set;
  if (count <= 1)
    set = bg_image_1_;
  else if (count == 2)
    set = bg_image_2_;
  else
    set = bg_image_3_;
  SetImage(BS_NORMAL, set->normal_image);
  SetImage(BS_HOT, set->hot_image);
  SetImage(BS_PUSHED, set->pushed_image);
  SchedulePaint();
}

void TabbedLauncherButton::OnPaint(gfx::Canvas* canvas) {
  ImageButton::OnPaint(canvas);

  hover_controller_.Draw(canvas, *bg_image_1_->normal_image);

  if (image_.empty() || !show_image_)
    return;

  bool save_layer = (animation_.get() && animation_->is_animating() &&
                     animation_->current_part_index() == 1);
  if (save_layer)
    canvas->SaveLayerAlpha(animation_->CurrentValueBetween(255, 0));

  int x = (width() - image_.width()) / 2;
  int y = (height() - image_.height()) / 2 + 1;
  canvas->DrawBitmapInt(image_, x, y);

  if (save_layer)
    canvas->Restore();
}

bool TabbedLauncherButton::OnMousePressed(const views::MouseEvent& event) {
  ImageButton::OnMousePressed(event);
  host_->MousePressedOnButton(this, event);
  hover_controller_.HideImmediately();
  return true;
}

void TabbedLauncherButton::OnMouseReleased(const views::MouseEvent& event) {
  host_->MouseReleasedOnButton(this, false);
  ImageButton::OnMouseReleased(event);
  hover_controller_.Show();
}

void TabbedLauncherButton::OnMouseCaptureLost() {
  host_->MouseReleasedOnButton(this, true);
  ImageButton::OnMouseCaptureLost();
  hover_controller_.Hide();
}

bool TabbedLauncherButton::OnMouseDragged(const views::MouseEvent& event) {
  ImageButton::OnMouseDragged(event);
  host_->MouseDraggedOnButton(this, event);
  return true;
}

void TabbedLauncherButton::OnMouseEntered(const views::MouseEvent& event) {
  ImageButton::OnMouseEntered(event);
  hover_controller_.Show();
}

void TabbedLauncherButton::OnMouseMoved(const views::MouseEvent& event) {
  ImageButton::OnMouseMoved(event);
  hover_controller_.SetLocation(event.location());
}

void TabbedLauncherButton::OnMouseExited(const views::MouseEvent& event) {
  ImageButton::OnMouseExited(event);
  hover_controller_.Hide();
}

// static
TabbedLauncherButton::ImageSet* TabbedLauncherButton::CreateImageSet(
    int normal_id,
    int pushed_id,
    int hot_id) {
  ImageSet* set = new ImageSet;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  set->normal_image = new SkBitmap(*rb.GetImageNamed(normal_id).ToSkBitmap());
  set->pushed_image = new SkBitmap(*rb.GetImageNamed(pushed_id).ToSkBitmap());
  set->hot_image = new SkBitmap(*rb.GetImageNamed(hot_id).ToSkBitmap());
  return set;
}

}  // namespace internal
}  // namespace ash
