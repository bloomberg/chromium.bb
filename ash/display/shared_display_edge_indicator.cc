// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/shared_display_edge_indicator.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {
namespace {

const int kIndicatorAnimationDurationMs = 1000;

views::Widget* CreateWidget(const gfx::Rect& bounds) {
  // This is just a placeholder and we'll use an image.
  views::Painter* painter = views::Painter::CreateHorizontalGradient(
      SK_ColorWHITE, SK_ColorWHITE);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  params.can_activate = false;
  params.keep_on_top = true;
  widget->set_focus_on_creation(false);
  widget->Init(params);
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget->GetNativeWindow()->SetName("SharedEdgeIndicator");
  views::View* content_view = new views::View;
  content_view->set_background(
      views::Background::CreateBackgroundPainter(true, painter));
  widget->SetContentsView(content_view);
  gfx::Display display = gfx::Screen::GetDisplayMatching(bounds);
  aura::Window* window = widget->GetNativeWindow();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(window->GetRootWindow());
  screen_position_client->SetBounds(window, bounds, display);
  widget->SetOpacity(0);
  widget->Show();
  return widget;
}

}  // namespace

SharedDisplayEdgeIndicator::SharedDisplayEdgeIndicator()
    : src_widget_(NULL),
      dst_widget_(NULL) {
}

SharedDisplayEdgeIndicator::~SharedDisplayEdgeIndicator() {
  Hide();
}

void SharedDisplayEdgeIndicator::Show(const gfx::Rect& src_bounds,
                                      const gfx::Rect& dst_bounds) {
  DCHECK(!src_widget_);
  DCHECK(!dst_widget_);
  src_widget_ = CreateWidget(src_bounds);
  dst_widget_ = CreateWidget(dst_bounds);
  animation_.reset(new ui::ThrobAnimation(this));
  animation_->SetThrobDuration(kIndicatorAnimationDurationMs);
  animation_->StartThrobbing(-1 /* infinite */);
}

void SharedDisplayEdgeIndicator::Hide() {
  if (src_widget_)
    src_widget_->Close();
  src_widget_ = NULL;
  if (dst_widget_)
    dst_widget_->Close();
  dst_widget_ = NULL;
}

void SharedDisplayEdgeIndicator::AnimationProgressed(
    const ui::Animation* animation) {
  int opacity = animation->CurrentValueBetween(0, 255);
  if (src_widget_)
    src_widget_->SetOpacity(opacity);
  if (dst_widget_)
    dst_widget_->SetOpacity(opacity);
}

}  // namespace internal
}  // namespace ash
