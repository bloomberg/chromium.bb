// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/phantom_window_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

// EdgePainter ----------------------------------------------------------------

namespace {

// Paints the background of the phantom window for window snapping.
class EdgePainter : public views::Painter {
 public:
  EdgePainter();
  virtual ~EdgePainter();

  // views::Painter:
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(EdgePainter);
};

}  // namespace


EdgePainter::EdgePainter() {
}

EdgePainter::~EdgePainter() {
}

gfx::Size EdgePainter::GetMinimumSize() const {
  return gfx::Size();
}

void EdgePainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  const int kInsetSize = 4;
  int x = kInsetSize;
  int y = kInsetSize;
  int w = size.width() - kInsetSize * 2;
  int h = size.height() - kInsetSize * 2;
  bool inset = (w > 0 && h > 0);
  if (!inset) {
    x = 0;
    y = 0;
    w = size.width();
    h = size.height();
  }
  SkPaint paint;
  paint.setColor(SkColorSetARGB(100, 0, 0, 0));
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  const int kRoundRectSize = 4;
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


// PhantomWindowController ----------------------------------------------------

PhantomWindowController::PhantomWindowController(aura::Window* window)
    : window_(window),
      phantom_below_window_(NULL),
      phantom_widget_(NULL),
      phantom_widget_start_(NULL) {
}

PhantomWindowController::~PhantomWindowController() {
  Hide();
}

void PhantomWindowController::Show(const gfx::Rect& bounds_in_screen) {
  if (bounds_in_screen == bounds_in_screen_)
    return;
  bounds_in_screen_ = bounds_in_screen;
  aura::Window* target_root = wm::GetRootWindowMatching(bounds_in_screen);
  // Show the phantom at the current bounds of the window. We'll animate to the
  // target bounds. If phantom exists, update the start bounds.
  if (!phantom_widget_)
    start_bounds_ = window_->GetBoundsInScreen();
  else
    start_bounds_ = phantom_widget_->GetWindowBoundsInScreen();
  if (phantom_widget_ &&
      phantom_widget_->GetNativeWindow()->GetRootWindow() != target_root) {
    phantom_widget_->Close();
    phantom_widget_ = NULL;
  }
  if (!phantom_widget_)
    phantom_widget_ = CreatePhantomWidget(target_root, start_bounds_);

  // Create a secondary widget in a second screen if start_bounds_ lie at least
  // partially in that other screen. This allows animations to start or restart
  // in one root window and progress into another root.
  aura::Window* start_root = wm::GetRootWindowMatching(start_bounds_);
  if (start_root == target_root) {
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    for (size_t i = 0; i < root_windows.size(); ++i) {
      if (root_windows[i] != target_root &&
          root_windows[i]->GetBoundsInScreen().Intersects(start_bounds_)) {
        start_root = root_windows[i];
        break;
      }
    }
  }
  if (phantom_widget_start_ &&
      (phantom_widget_start_->GetNativeWindow()->GetRootWindow() != start_root
       || start_root == target_root)) {
    phantom_widget_start_->Close();
    phantom_widget_start_ = NULL;
  }
  if (!phantom_widget_start_ && start_root != target_root)
    phantom_widget_start_ = CreatePhantomWidget(start_root, start_bounds_);

  animation_.reset(new gfx::SlideAnimation(this));
  animation_->SetTweenType(gfx::Tween::EASE_IN);
  const int kAnimationDurationMS = 200;
  animation_->SetSlideDuration(kAnimationDurationMS);
  animation_->Show();
}

void PhantomWindowController::Hide() {
  if (phantom_widget_)
    phantom_widget_->Close();
  phantom_widget_ = NULL;
  if (phantom_widget_start_)
    phantom_widget_start_->Close();
  phantom_widget_start_ = NULL;
}

bool PhantomWindowController::IsShowing() const {
  return phantom_widget_ != NULL;
}

void PhantomWindowController::AnimationProgressed(
    const gfx::Animation* animation) {
  const gfx::Rect current_bounds =
      animation->CurrentValueBetween(start_bounds_, bounds_in_screen_);
  if (phantom_widget_start_)
    phantom_widget_start_->SetBounds(current_bounds);
  phantom_widget_->SetBounds(current_bounds);
}

views::Widget* PhantomWindowController::CreatePhantomWidget(
    aura::Window* root_window,
    const gfx::Rect& bounds_in_screen) {
  views::Widget* phantom_widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  // PhantomWindowController is used by FrameMaximizeButton to highlight the
  // launcher button. Put the phantom in the same window as the launcher so that
  // the phantom is visible.
  params.parent = Shell::GetContainer(root_window,
                                      kShellWindowId_ShelfContainer);
  params.can_activate = false;
  params.keep_on_top = true;
  phantom_widget->set_focus_on_creation(false);
  phantom_widget->Init(params);
  phantom_widget->SetVisibilityChangedAnimationsEnabled(false);
  phantom_widget->GetNativeWindow()->SetName("PhantomWindow");
  phantom_widget->GetNativeWindow()->set_id(kShellWindowId_PhantomWindow);
  views::View* content_view = new views::View;
  content_view->set_background(
      views::Background::CreateBackgroundPainter(true, new EdgePainter));
  phantom_widget->SetContentsView(content_view);
  phantom_widget->SetBounds(bounds_in_screen);
  if (phantom_below_window_)
    phantom_widget->StackBelow(phantom_below_window_);
  else
    phantom_widget->StackAbove(window_);

  // Show the widget after all the setups.
  phantom_widget->Show();

  // Fade the window in.
  ui::Layer* widget_layer = phantom_widget->GetNativeWindow()->layer();
  widget_layer->SetOpacity(0);
  ui::ScopedLayerAnimationSettings scoped_setter(widget_layer->GetAnimator());
  widget_layer->SetOpacity(1);
  return phantom_widget;
}

}  // namespace internal
}  // namespace ash
