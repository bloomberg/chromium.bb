// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/phantom_window_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
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

// Paints the background of the phantom window.
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
    : window_(window) {
}

PhantomWindowController::~PhantomWindowController() {
  Hide();
}

void PhantomWindowController::Show(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;
  bounds_ = bounds;
  if (!phantom_widget_.get()) {
    // Show the phantom at the bounds of the window. We'll animate to the target
    // bounds.
    start_bounds_ = window_->bounds();
    CreatePhantomWidget(start_bounds_);
  } else {
    start_bounds_ = phantom_widget_->GetWindowScreenBounds();
  }
  animation_.reset(new ui::SlideAnimation(this));
  animation_->Show();
}

void PhantomWindowController::Hide() {
  phantom_widget_.reset();
}

bool PhantomWindowController::IsShowing() const {
  return phantom_widget_.get() != NULL;
}

void PhantomWindowController::AnimationProgressed(
    const ui::Animation* animation) {
  phantom_widget_->SetBounds(
      animation->CurrentValueBetween(start_bounds_, bounds_));
}

void PhantomWindowController::CreatePhantomWidget(const gfx::Rect& bounds) {
  DCHECK(!phantom_widget_.get());
  phantom_widget_.reset(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // PhantomWindowController is used by FrameMaximizeButton to highlight the
  // launcher button. Put the phantom in the same window as the launcher so that
  // the phantom is visible.
  params.parent =
      Shell::GetInstance()->GetContainer(kShellWindowId_LauncherContainer);
  params.can_activate = false;
  params.keep_on_top = true;
  phantom_widget_->set_focus_on_creation(false);
  phantom_widget_->Init(params);
  phantom_widget_->SetVisibilityChangedAnimationsEnabled(false);
  phantom_widget_->GetNativeWindow()->SetName("PhantomWindow");
  views::View* content_view = new views::View;
  content_view->set_background(
      views::Background::CreateBackgroundPainter(true, new EdgePainter));
  phantom_widget_->SetContentsView(content_view);
  phantom_widget_->SetBounds(bounds);
  phantom_widget_->StackAbove(window_);
  phantom_widget_->Show();
  // Fade the window in.
  ui::Layer* layer = phantom_widget_->GetNativeWindow()->layer();
  layer->SetOpacity(0);
  ui::ScopedLayerAnimationSettings scoped_setter(layer->GetAnimator());
  layer->SetOpacity(1);
}

}  // namespace internal
}  // namespace ash
