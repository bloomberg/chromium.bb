// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/frame/header_view.h"

#include "ash/common/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/common/frame/default_header_painter.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

HeaderView::HeaderView(views::Widget* target_widget)
    : target_widget_(target_widget),
      header_painter_(new DefaultHeaderPainter),
      avatar_icon_(nullptr),
      caption_button_container_(nullptr),
      fullscreen_visible_fraction_(0) {
  caption_button_container_ =
      new FrameCaptionButtonContainerView(target_widget_);
  caption_button_container_->UpdateSizeButtonVisibility();
  AddChildView(caption_button_container_);

  header_painter_->Init(target_widget_, this, caption_button_container_);
  UpdateAvatarIcon();

  WmShell::Get()->AddShellObserver(this);
}

HeaderView::~HeaderView() {
  WmShell::Get()->RemoveShellObserver(this);
}

void HeaderView::SchedulePaintForTitle() {
  header_painter_->SchedulePaintForTitle();
}

void HeaderView::ResetWindowControls() {
  caption_button_container_->ResetWindowControls();
}

int HeaderView::GetPreferredOnScreenHeight() {
  if (is_immersive_delegate_ && target_widget_->IsFullscreen()) {
    return static_cast<int>(GetPreferredHeight() *
                            fullscreen_visible_fraction_);
  }
  return GetPreferredHeight();
}

int HeaderView::GetPreferredHeight() {
  // Calculating the preferred height requires at least one Layout().
  if (!did_layout_)
    Layout();
  return header_painter_->GetHeaderHeightForPainting();
}

int HeaderView::GetMinimumWidth() const {
  return header_painter_->GetMinimumHeaderWidth();
}

void HeaderView::UpdateAvatarIcon() {
  SessionStateDelegate* delegate = WmShell::Get()->GetSessionStateDelegate();
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(target_widget_);
  bool show = delegate->ShouldShowAvatar(window);
  if (!show) {
    if (!avatar_icon_)
      return;
    delete avatar_icon_;
    avatar_icon_ = nullptr;
  } else {
    gfx::ImageSkia image = delegate->GetAvatarImageForWindow(window);
    DCHECK_EQ(image.width(), image.height());
    if (!avatar_icon_) {
      avatar_icon_ = new views::ImageView();
      AddChildView(avatar_icon_);
    }
    avatar_icon_->SetImage(image);
  }
  header_painter_->UpdateLeftHeaderView(avatar_icon_);
  Layout();
}

void HeaderView::SizeConstraintsChanged() {
  caption_button_container_->ResetWindowControls();
  caption_button_container_->UpdateSizeButtonVisibility();
  Layout();
}

void HeaderView::SetFrameColors(SkColor active_frame_color,
                                SkColor inactive_frame_color) {
  header_painter_->SetFrameColors(active_frame_color, inactive_frame_color);
}

SkColor HeaderView::GetActiveFrameColor() const {
  return header_painter_->GetActiveFrameColor();
}

SkColor HeaderView::GetInactiveFrameColor() const {
  return header_painter_->GetInactiveFrameColor();
}

///////////////////////////////////////////////////////////////////////////////
// HeaderView, views::View overrides:

void HeaderView::Layout() {
  did_layout_ = true;
  header_painter_->LayoutHeader();
}

void HeaderView::OnPaint(gfx::Canvas* canvas) {
  bool paint_as_active =
      target_widget_->non_client_view()->frame_view()->ShouldPaintAsActive();
  caption_button_container_->SetPaintAsActive(paint_as_active);

  HeaderPainter::Mode header_mode = paint_as_active
                                        ? HeaderPainter::MODE_ACTIVE
                                        : HeaderPainter::MODE_INACTIVE;
  header_painter_->PaintHeader(canvas, header_mode);
}

void HeaderView::ChildPreferredSizeChanged(views::View* child) {
  // FrameCaptionButtonContainerView animates the visibility changes in
  // UpdateSizeButtonVisibility(false). Due to this a new size is not available
  // until the completion of the animation. Layout in response to the preferred
  // size changes.
  if (child != caption_button_container_)
    return;
  parent()->Layout();
}

///////////////////////////////////////////////////////////////////////////////
// HeaderView, ShellObserver overrides:

void HeaderView::OnOverviewModeStarting() {
  caption_button_container_->SetVisible(false);
}

void HeaderView::OnOverviewModeEnded() {
  caption_button_container_->SetVisible(true);
}

void HeaderView::OnMaximizeModeStarted() {
  caption_button_container_->UpdateSizeButtonVisibility();
  parent()->Layout();
}

void HeaderView::OnMaximizeModeEnded() {
  caption_button_container_->UpdateSizeButtonVisibility();
  parent()->Layout();
}

views::View* HeaderView::avatar_icon() const {
  return avatar_icon_;
}

///////////////////////////////////////////////////////////////////////////////
// HeaderView,
//   ImmersiveFullscreenControllerDelegate overrides:

void HeaderView::OnImmersiveRevealStarted() {
  fullscreen_visible_fraction_ = 0;
  SetPaintToLayer();
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
