// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/header_view.h"

#include <memory>

#include "ash/frame/caption_buttons/frame_back_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/custom_frame_view_ash.h"
#include "ash/frame/default_frame_header.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

HeaderView::HeaderView(views::Widget* target_widget,
                       mojom::WindowStyle window_style)
    : target_widget_(target_widget),
      avatar_icon_(nullptr),
      caption_button_container_(nullptr),
      fullscreen_visible_fraction_(0),
      should_paint_(true) {
  caption_button_container_ =
      new FrameCaptionButtonContainerView(target_widget_);
  caption_button_container_->UpdateSizeButtonVisibility();
  AddChildView(caption_button_container_);

  frame_header_ = std::make_unique<DefaultFrameHeader>(
      target_widget_, this, caption_button_container_, window_style);

  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

HeaderView::~HeaderView() {
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
}

void HeaderView::SchedulePaintForTitle() {
  frame_header_->SchedulePaintForTitle();
}

void HeaderView::ResetWindowControls() {
  caption_button_container_->ResetWindowControls();
}

int HeaderView::GetPreferredOnScreenHeight() {
  const bool should_hide_titlebar_in_tablet_mode =
      Shell::Get()->tablet_mode_controller() &&
      Shell::Get()->tablet_mode_controller()->ShouldAutoHideTitlebars(
          target_widget_);

  if (is_immersive_delegate_ &&
      (target_widget_->IsFullscreen() || should_hide_titlebar_in_tablet_mode)) {
    return static_cast<int>(GetPreferredHeight() *
                            fullscreen_visible_fraction_);
  }
  return GetPreferredHeight();
}

int HeaderView::GetPreferredHeight() {
  // Calculating the preferred height requires at least one Layout().
  if (!did_layout_)
    Layout();
  return frame_header_->GetHeaderHeightForPainting();
}

int HeaderView::GetMinimumWidth() const {
  return frame_header_->GetMinimumHeaderWidth();
}

void HeaderView::SetAvatarIcon(const gfx::ImageSkia& avatar) {
  const bool show = !avatar.isNull();
  if (!show) {
    if (!avatar_icon_)
      return;
    delete avatar_icon_;
    avatar_icon_ = nullptr;
  } else {
    DCHECK_EQ(avatar.width(), avatar.height());
    if (!avatar_icon_) {
      avatar_icon_ = new views::ImageView();
      AddChildView(avatar_icon_);
    }
    avatar_icon_->SetImage(avatar);
  }
  frame_header_->set_left_header_view(avatar_icon_);
  Layout();
}

void HeaderView::SetBackButtonState(FrameBackButtonState state) {
  if (state != FrameBackButtonState::kInvisible) {
    if (!back_button_) {
      back_button_ = new FrameBackButton();
      AddChildView(back_button_);
    }
    back_button_->SetEnabled(state == FrameBackButtonState::kVisibleEnabled);
  } else {
    delete back_button_;
    back_button_ = nullptr;
  }
  frame_header_->set_back_button(back_button_);
  Layout();
}

void HeaderView::SizeConstraintsChanged() {
  caption_button_container_->ResetWindowControls();
  caption_button_container_->UpdateSizeButtonVisibility();
  Layout();
}

void HeaderView::SetFrameColors(SkColor active_frame_color,
                                SkColor inactive_frame_color) {
  frame_header_->SetFrameColors(active_frame_color, inactive_frame_color);
}

SkColor HeaderView::GetActiveFrameColor() const {
  return frame_header_->GetActiveFrameColor();
}

SkColor HeaderView::GetInactiveFrameColor() const {
  return frame_header_->GetInactiveFrameColor();
}

///////////////////////////////////////////////////////////////////////////////
// HeaderView, views::View overrides:

void HeaderView::Layout() {
  did_layout_ = true;
  if (back_button_)
    back_button_->set_use_light_images(frame_header_->ShouldUseLightImages());
  frame_header_->LayoutHeader();
}

void HeaderView::OnPaint(gfx::Canvas* canvas) {
  if (!should_paint_)
    return;

  bool paint_as_active =
      target_widget_->non_client_view()->frame_view()->ShouldPaintAsActive();
  frame_header_->SetPaintAsActive(paint_as_active);

  FrameHeader::Mode header_mode =
      paint_as_active ? FrameHeader::MODE_ACTIVE : FrameHeader::MODE_INACTIVE;
  frame_header_->PaintHeader(canvas, header_mode);
}

void HeaderView::ChildPreferredSizeChanged(views::View* child) {
  if (child != caption_button_container_)
    return;

  // May be null during view initialization.
  if (parent())
    parent()->Layout();
}

void HeaderView::OnTabletModeStarted() {
  caption_button_container_->UpdateSizeButtonVisibility();
  parent()->Layout();
  if (Shell::Get()->tablet_mode_controller()->ShouldAutoHideTitlebars(
          nullptr)) {
    target_widget_->non_client_view()->Layout();
  }
}

void HeaderView::OnTabletModeEnded() {
  caption_button_container_->UpdateSizeButtonVisibility();
  parent()->Layout();
  target_widget_->non_client_view()->Layout();
}

views::View* HeaderView::avatar_icon() const {
  return avatar_icon_;
}

void HeaderView::SetShouldPaintHeader(bool paint) {
  if (should_paint_ == paint)
    return;

  should_paint_ = paint;
  caption_button_container_->SetVisible(should_paint_);
  SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// HeaderView,
//   ImmersiveFullscreenControllerDelegate overrides:

void HeaderView::OnImmersiveRevealStarted() {
  fullscreen_visible_fraction_ = 0;
  SetPaintToLayer();
  // The immersive layer should always be top.
  layer()->parent()->StackAtTop(layer());
  parent()->Layout();
}

void HeaderView::OnImmersiveRevealEnded() {
  fullscreen_visible_fraction_ = 0;
  DestroyLayer();
  parent()->Layout();
}

void HeaderView::OnImmersiveFullscreenExited() {
  fullscreen_visible_fraction_ = 0;
  DestroyLayer();
  parent()->Layout();
}

void HeaderView::SetVisibleFraction(double visible_fraction) {
  if (fullscreen_visible_fraction_ != visible_fraction) {
    fullscreen_visible_fraction_ = visible_fraction;
    parent()->Layout();
  }
}

std::vector<gfx::Rect> HeaderView::GetVisibleBoundsInScreen() const {
  // TODO(pkotwicz): Implement views::View::ConvertRectToScreen().
  gfx::Rect visible_bounds(GetVisibleBounds());
  gfx::Point visible_origin_in_screen(visible_bounds.origin());
  views::View::ConvertPointToScreen(this, &visible_origin_in_screen);
  std::vector<gfx::Rect> bounds_in_screen;
  bounds_in_screen.push_back(
      gfx::Rect(visible_origin_in_screen, visible_bounds.size()));
  return bounds_in_screen;
}

}  // namespace ash
