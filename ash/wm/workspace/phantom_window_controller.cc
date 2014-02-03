// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/phantom_window_controller.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

// The duration of the show animation.
const int kAnimationDurationMs = 200;

// The size of the phantom window at the beginning of the show animation in
// relation to the size of the phantom window at the end of the animation when
// using the alternate caption button style.
const float kAlternateCaptionButtonStyleAnimationInitialBoundsRatio = 0.85f;

// The amount of pixels that the phantom window's shadow should extend past
// the bounds passed into Show(). There is no shadow when not using the
// alternate caption button style.
const int kShadowThickness = 15;

// Converts the bounds of a phantom window without a shadow to those of a
// phantom window with a shadow.
gfx::Rect GetBoundsWithShadow(const gfx::Rect& bounds) {
  gfx::Rect bounds_with_shadow(bounds);
  // Phantom windows have a shadow solely when using the alternate caption
  // button style.
  if (switches::UseAlternateFrameCaptionButtonStyle())
    bounds_with_shadow.Inset(-kShadowThickness, -kShadowThickness);
  return bounds_with_shadow;
}

// Starts an animation of |widget| to |new_bounds_in_screen|. No-op if |widget|
// is NULL.
void AnimateToBounds(views::Widget* widget,
                     const gfx::Rect& new_bounds_in_screen) {
  if (!widget)
    return;

  ui::ScopedLayerAnimationSettings scoped_setter(
      widget->GetNativeWindow()->layer()->GetAnimator());
  scoped_setter.SetTweenType(gfx::Tween::EASE_IN);
  scoped_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  scoped_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
  widget->SetBounds(new_bounds_in_screen);
}

// EdgePainter ----------------------------------------------------------------

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

}  // namespace

// PhantomWindowController ----------------------------------------------------

PhantomWindowController::PhantomWindowController(aura::Window* window)
    : window_(window),
      phantom_below_window_(NULL) {
}

PhantomWindowController::~PhantomWindowController() {
}

void PhantomWindowController::Show(const gfx::Rect& bounds_in_screen) {
  if (GetBoundsWithShadow(bounds_in_screen) == target_bounds_in_screen_)
    return;
  target_bounds_in_screen_ = GetBoundsWithShadow(bounds_in_screen);

  if (switches::UseAlternateFrameCaptionButtonStyle()) {
    gfx::Rect start_bounds_in_screen = target_bounds_in_screen_;
    float inset_ratio =
        (1.0f - kAlternateCaptionButtonStyleAnimationInitialBoundsRatio) / 2;
    start_bounds_in_screen.Inset(
        static_cast<int>(start_bounds_in_screen.width() * inset_ratio),
        static_cast<int>(start_bounds_in_screen.height() * inset_ratio));
    phantom_widget_in_target_root_ = CreatePhantomWidget(
        wm::GetRootWindowMatching(target_bounds_in_screen_),
        start_bounds_in_screen);

    AnimateToBounds(phantom_widget_in_target_root_.get(),
                    target_bounds_in_screen_);
  } else {
    gfx::Rect start_bounds_in_screen;
    if (!phantom_widget_in_target_root_) {
      start_bounds_in_screen =
          GetBoundsWithShadow(window_->GetBoundsInScreen());
    } else {
      start_bounds_in_screen =
          phantom_widget_in_target_root_->GetWindowBoundsInScreen();
    }

    aura::Window* target_root =
        wm::GetRootWindowMatching(target_bounds_in_screen_);
    if (!phantom_widget_in_target_root_ ||
        phantom_widget_in_target_root_->GetNativeWindow()->GetRootWindow() !=
            target_root) {
      phantom_widget_in_target_root_ =
          CreatePhantomWidget(target_root, start_bounds_in_screen);
    }
    AnimateToBounds(phantom_widget_in_target_root_.get(),
                    target_bounds_in_screen_);

    // Create a secondary widget in a second screen if |start_bounds_in_screen|
    // lies at least partially in another screen. This allows animations to
    // start or restart in one root window and progress to another root.
    aura::Window* start_root =
        wm::GetRootWindowMatching(start_bounds_in_screen);
    if (start_root == target_root) {
      aura::Window::Windows root_windows = Shell::GetAllRootWindows();
      for (size_t i = 0; i < root_windows.size(); ++i) {
        if (root_windows[i] != target_root &&
            root_windows[i]->GetBoundsInScreen().Intersects(
                start_bounds_in_screen)) {
          start_root = root_windows[i];
          break;
        }
      }
    }
    if (start_root == target_root) {
      phantom_widget_in_start_root_.reset();
    } else {
      if (!phantom_widget_in_start_root_ ||
          phantom_widget_in_start_root_->GetNativeWindow()->GetRootWindow() !=
              start_root) {
        phantom_widget_in_start_root_ =
            CreatePhantomWidget(start_root, start_bounds_in_screen);
      }
      AnimateToBounds(phantom_widget_in_start_root_.get(),
                      target_bounds_in_screen_);
    }
  }
}

scoped_ptr<views::Widget> PhantomWindowController::CreatePhantomWidget(
    aura::Window* root_window,
    const gfx::Rect& bounds_in_screen) {
  scoped_ptr<views::Widget> phantom_widget(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  // PhantomWindowController is used by FrameMaximizeButton to highlight the
  // launcher button. Put the phantom in the same window as the launcher so that
  // the phantom is visible.
  params.parent = Shell::GetContainer(root_window,
                                      kShellWindowId_ShelfContainer);
  params.can_activate = false;
  params.keep_on_top = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  phantom_widget->set_focus_on_creation(false);
  phantom_widget->Init(params);
  phantom_widget->SetVisibilityChangedAnimationsEnabled(false);
  phantom_widget->GetNativeWindow()->SetName("PhantomWindow");
  phantom_widget->GetNativeWindow()->set_id(kShellWindowId_PhantomWindow);
  phantom_widget->SetBounds(bounds_in_screen);
  if (phantom_below_window_)
    phantom_widget->StackBelow(phantom_below_window_);
  else
    phantom_widget->StackAbove(window_);

  views::Painter* background_painter = NULL;
  if (switches::UseAlternateFrameCaptionButtonStyle()) {
    const int kImages[] = IMAGE_GRID(IDR_AURA_PHANTOM_WINDOW);
    background_painter = views::Painter::CreateImageGridPainter(kImages);
  } else {
    background_painter = new EdgePainter;
  }
  views::View* content_view = new views::View;
  content_view->set_background(
      views::Background::CreateBackgroundPainter(true, background_painter));
  phantom_widget->SetContentsView(content_view);

  // Show the widget after all the setups.
  phantom_widget->Show();

  // Fade the window in.
  ui::Layer* widget_layer = phantom_widget->GetNativeWindow()->layer();
  widget_layer->SetOpacity(0);
  ui::ScopedLayerAnimationSettings scoped_setter(widget_layer->GetAnimator());
  scoped_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
  widget_layer->SetOpacity(1);

  return phantom_widget.Pass();
}

}  // namespace internal
}  // namespace ash
