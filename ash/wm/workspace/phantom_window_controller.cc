// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/phantom_window_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/aura/window.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

// Amount to inset from the bounds for EdgePainter.
const int kInsetSize = 4;

// Size of the round rect used by EdgePainter.
const int kRoundRectSize = 4;

// Animation time for the phantom window state change.
const int kAnimationDuration = 200;

// Paints the background of the phantom window for window snapping.
class EdgePainter : public views::Painter {
 public:
  EdgePainter() {}

  // views::Painter overrides:
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE {
    int x = kInsetSize;
    int y = kInsetSize;
    int w = size.width() - kInsetSize * 2;
    int h = size.height() - kInsetSize * 2;
    bool inset = (w > 0 && h > 0);
    if (w < 0 || h < 0) {
      x = 0;
      y = 0;
      w = size.width();
      h = size.height();
    }
    SkPaint paint;
    paint.setColor(SkColorSetARGB(100, 0, 0, 0));
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    canvas->sk_canvas()->drawRoundRect(
        gfx::RectToSkRect(gfx::Rect(x, y, w, h)),
        SkIntToScalar(kRoundRectSize), SkIntToScalar(kRoundRectSize), paint);
    if (!inset)
      return;

    paint.setColor(SkColorSetARGB(200, 255, 255, 255));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkIntToScalar(2));
    canvas->sk_canvas()->drawRoundRect(
        gfx::RectToSkRect(gfx::Rect(x, y, w, h)), SkIntToScalar(kRoundRectSize),
        SkIntToScalar(kRoundRectSize), paint);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EdgePainter);
};

}  // namespace

PhantomWindowController::PhantomWindowController(aura::Window* window)
    : window_(window),
      phantom_below_window_(NULL),
      phantom_widget_(NULL) {
}

PhantomWindowController::~PhantomWindowController() {
  Hide();
}

void PhantomWindowController::Show(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;
  bounds_ = bounds;
  if (!phantom_widget_) {
    // Show the phantom at the bounds of the window. We'll animate to the target
    // bounds.
    start_bounds_ = window_->GetBoundsInScreen();
    CreatePhantomWidget(start_bounds_);
  } else {
    start_bounds_ = phantom_widget_->GetWindowBoundsInScreen();
  }
  animation_.reset(new ui::SlideAnimation(this));
  animation_->SetTweenType(ui::Tween::EASE_IN);
  animation_->SetSlideDuration(kAnimationDuration);
  animation_->Show();
}

void PhantomWindowController::Hide() {
  if (phantom_widget_)
    phantom_widget_->Close();
  phantom_widget_ = NULL;
}

bool PhantomWindowController::IsShowing() const {
  return phantom_widget_ != NULL;
}

void PhantomWindowController::AnimationProgressed(
    const ui::Animation* animation) {
  phantom_widget_->SetBounds(
      animation->CurrentValueBetween(start_bounds_, bounds_));
}

void PhantomWindowController::CreatePhantomWidget(const gfx::Rect& bounds) {
  DCHECK(!phantom_widget_);
  phantom_widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  // PhantomWindowController is used by FrameMaximizeButton to highlight the
  // launcher button. Put the phantom in the same window as the launcher so that
  // the phantom is visible.
  params.parent = Shell::GetContainer(wm::GetRootWindowMatching(bounds),
                                      kShellWindowId_ShelfContainer);
  params.can_activate = false;
  params.keep_on_top = true;
  phantom_widget_->set_focus_on_creation(false);
  phantom_widget_->Init(params);
  phantom_widget_->SetVisibilityChangedAnimationsEnabled(false);
  phantom_widget_->GetNativeWindow()->SetName("PhantomWindow");
  phantom_widget_->GetNativeWindow()->set_id(kShellWindowId_PhantomWindow);
  views::View* content_view = new views::View;
  content_view->set_background(
      views::Background::CreateBackgroundPainter(true, new EdgePainter));
  phantom_widget_->SetContentsView(content_view);
  phantom_widget_->SetBounds(bounds);
  if (phantom_below_window_)
    phantom_widget_->StackBelow(phantom_below_window_);
  else
    phantom_widget_->StackAbove(window_);

  // Show the widget after all the setups.
  phantom_widget_->Show();

  // Fade the window in.
  ui::Layer* widget_layer = phantom_widget_->GetNativeWindow()->layer();
  widget_layer->SetOpacity(0);
  ui::ScopedLayerAnimationSettings scoped_setter(widget_layer->GetAnimator());
  widget_layer->SetOpacity(1);
}

}  // namespace internal
}  // namespace ash
