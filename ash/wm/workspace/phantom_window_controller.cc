// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/phantom_window_controller.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

const int kRoundRectSize = 3;

// Paints the background of the phantom window.
class BackgroundPainter : public views::Painter {
 public:
  BackgroundPainter() {}

  // views::Painter overrides:
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE {
    canvas->AsCanvasSkia()->DrawDashedRect(
        gfx::Rect(0, 0, size.width(), size.height()),
        SkColorSetARGB(128, 0, 0, 0));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundPainter);
};

// Used to delete the widget after a delay, or if the window is deleted.
class DelayedWidgetDeleter : public aura::WindowObserver {
 public:
  DelayedWidgetDeleter(views::Widget* widget, aura::Window* window)
      : widget_(widget),
        window_(window) {
    window->AddObserver(this);
  }

  // Schedules deletion of the widget passed to the constructor after |delay_ms|
  // milliseconds.
  void ScheduleDelete(int delay_ms) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(delay_ms),
                 this, &DelayedWidgetDeleter::TimerFired);
  }

  // aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    delete this;
  }

 private:
  virtual ~DelayedWidgetDeleter() {
    window_->RemoveObserver(this);
  }

  void TimerFired() {
    delete this;
  }

  scoped_ptr<views::Widget> widget_;

  aura::Window* window_;

  base::OneShotTimer<DelayedWidgetDeleter> timer_;

  DISALLOW_COPY_AND_ASSIGN(DelayedWidgetDeleter);
};

}  // namespace

PhantomWindowController::PhantomWindowController(aura::Window* window,
                                                 int delay_ms)
    : window_(window),
      delay_ms_(delay_ms),
      phantom_widget_(NULL) {
}

PhantomWindowController::~PhantomWindowController() {
  Hide();
}

void PhantomWindowController::Show(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;
  if (phantom_widget_.get())
    phantom_widget_->Hide();
  bounds_ = bounds;
  show_timer_.Stop();
  show_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(delay_ms_),
                    this, &PhantomWindowController::ShowNow);
}

void PhantomWindowController::Hide() {
  phantom_widget_.reset();
  show_timer_.Stop();
}

void PhantomWindowController::DelayedClose(int delay_ms) {
  show_timer_.Stop();
  if (!phantom_widget_.get() || !phantom_widget_->IsVisible())
    return;

  // DelayedWidgetDeleter deletes itself after the delay.
  DelayedWidgetDeleter* deleter =
      new DelayedWidgetDeleter(phantom_widget_.release(), window_);
  deleter->ScheduleDelete(delay_ms);
}

void PhantomWindowController::ShowNow() {
  if (!phantom_widget_.get()) {
    phantom_widget_.reset(new views::Widget);
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.transparent = true;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = window_->parent();
    params.can_activate = false;
    phantom_widget_->set_focus_on_creation(false);
    phantom_widget_->Init(params);
    phantom_widget_->SetVisibilityChangedAnimationsEnabled(false);
    phantom_widget_->GetNativeWindow()->SetName("PhantomWindow");
    views::View* content_view = new views::View;
    content_view->set_background(
        views::Background::CreateBackgroundPainter(true,
                                                   new BackgroundPainter));
    phantom_widget_->SetContentsView(content_view);
    phantom_widget_->SetBounds(bounds_);
    phantom_widget_->StackBelow(window_);
    phantom_widget_->Show();
    return;
  }

  phantom_widget_->SetBounds(bounds_);
  phantom_widget_->Show();
  // Fade the window in.
  ui::Layer* layer = phantom_widget_->GetNativeWindow()->layer();
  layer->SetOpacity(0);
  ui::ScopedLayerAnimationSettings scoped_setter(layer->GetAnimator());
  layer->SetOpacity(1);
}

}  // namespace internal
}  // namespace ash
