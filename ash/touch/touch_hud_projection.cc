// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_hud_projection.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"

namespace ash {

const int kPointRadius = 20;
const SkColor kProjectionFillColor = SkColorSetRGB(0xF5, 0xF5, 0xDC);
const SkColor kProjectionStrokeColor = SK_ColorGRAY;
const int kProjectionAlpha = 0xB0;
const int kFadeoutDurationInMs = 250;
const int kFadeoutFrameRate = 60;

// TouchPointView draws a single touch point. This object manages its own
// lifetime and deletes itself upon fade-out completion or whenever |Remove()|
// is explicitly called.
class TouchPointView : public views::View,
                       public gfx::AnimationDelegate,
                       public views::WidgetObserver {
 public:
  explicit TouchPointView(views::Widget* parent_widget)
      : circle_center_(kPointRadius + 1, kPointRadius + 1),
        gradient_center_(SkPoint::Make(kPointRadius + 1,
                                       kPointRadius + 1)) {
    SetPaintToLayer(true);
    SetFillsBoundsOpaquely(false);

    SetSize(gfx::Size(2 * kPointRadius + 2, 2 * kPointRadius + 2));

    stroke_paint_.setStyle(SkPaint::kStroke_Style);
    stroke_paint_.setColor(kProjectionStrokeColor);

    gradient_colors_[0] = kProjectionFillColor;
    gradient_colors_[1] = kProjectionStrokeColor;

    gradient_pos_[0] = SkFloatToScalar(0.9f);
    gradient_pos_[1] = SkFloatToScalar(1.0f);

    parent_widget->GetContentsView()->AddChildView(this);

    parent_widget->AddObserver(this);
  }

  void UpdateTouch(const ui::TouchEvent& touch) {
    if (touch.type() == ui::ET_TOUCH_RELEASED ||
        touch.type() == ui::ET_TOUCH_CANCELLED) {
      fadeout_.reset(new gfx::LinearAnimation(kFadeoutDurationInMs,
                                             kFadeoutFrameRate,
                                             this));
      fadeout_->Start();
    } else {
      SetX(parent()->GetMirroredXInView(touch.root_location().x()) -
               kPointRadius - 1);
      SetY(touch.root_location().y() - kPointRadius - 1);
    }
  }

  void Remove() {
    delete this;
  }

 private:
  virtual ~TouchPointView() {
    GetWidget()->RemoveObserver(this);
    parent()->RemoveChildView(this);
  }

  // Overridden from views::View.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    int alpha = kProjectionAlpha;
    if (fadeout_)
      alpha = static_cast<int>(fadeout_->CurrentValueBetween(alpha, 0));
    fill_paint_.setAlpha(alpha);
    stroke_paint_.setAlpha(alpha);
    SkShader* shader = SkGradientShader::CreateRadial(
        gradient_center_,
        SkIntToScalar(kPointRadius),
        gradient_colors_,
        gradient_pos_,
        arraysize(gradient_colors_),
        SkShader::kMirror_TileMode);
    fill_paint_.setShader(shader);
    shader->unref();
    canvas->DrawCircle(circle_center_, SkIntToScalar(kPointRadius),
                       fill_paint_);
    canvas->DrawCircle(circle_center_, SkIntToScalar(kPointRadius),
                       stroke_paint_);
  }

  // Overridden from gfx::AnimationDelegate.
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE {
    DCHECK_EQ(fadeout_.get(), animation);
    delete this;
  }

  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE {
    DCHECK_EQ(fadeout_.get(), animation);
    SchedulePaint();
  }

  virtual void AnimationCanceled(const gfx::Animation* animation) OVERRIDE {
    AnimationEnded(animation);
  }

  // Overridden from views::WidgetObserver.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    if (fadeout_)
      fadeout_->Stop();
    else
      Remove();
  }

  const gfx::Point circle_center_;
  const SkPoint gradient_center_;

  SkPaint fill_paint_;
  SkPaint stroke_paint_;
  SkColor gradient_colors_[2];
  SkScalar gradient_pos_[2];

  scoped_ptr<gfx::Animation> fadeout_;

  DISALLOW_COPY_AND_ASSIGN(TouchPointView);
};

TouchHudProjection::TouchHudProjection(aura::Window* initial_root)
    : TouchObserverHUD(initial_root) {
}

TouchHudProjection::~TouchHudProjection() {
}

void TouchHudProjection::Clear() {
  for (std::map<int, TouchPointView*>::iterator iter = points_.begin();
      iter != points_.end(); iter++)
    iter->second->Remove();
  points_.clear();
}

void TouchHudProjection::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED) {
    TouchPointView* point = new TouchPointView(widget());
    point->UpdateTouch(*event);
    std::pair<std::map<int, TouchPointView*>::iterator, bool> result =
        points_.insert(std::make_pair(event->touch_id(), point));
    // If a |TouchPointView| is already mapped to the touch id, remove it and
    // replace it with the new one.
    if (!result.second) {
      result.first->second->Remove();
      result.first->second = point;
    }
  } else {
    std::map<int, TouchPointView*>::iterator iter =
        points_.find(event->touch_id());
    if (iter != points_.end()) {
      iter->second->UpdateTouch(*event);
      if (event->type() == ui::ET_TOUCH_RELEASED ||
          event->type() == ui::ET_TOUCH_CANCELLED)
        points_.erase(iter);
    }
  }
}

void TouchHudProjection::SetHudForRootWindowController(
    RootWindowController* controller) {
  controller->set_touch_hud_projection(this);
}

void TouchHudProjection::UnsetHudForRootWindowController(
    RootWindowController* controller) {
  controller->set_touch_hud_projection(NULL);
}

}  // namespace ash
