// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ANIMATION_H_
#define CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ANIMATION_H_

#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"

namespace gfx {
class Canvas;
class PointF;
}  // namespace gfx

// Delegate class for AppMenuAnimation. The delegate is expected to
// handle animation events and invalidate the platform's view.
class AppMenuAnimationDelegate {
 public:
  // Called when the animation has started/ended.
  virtual void AppMenuAnimationStarted() = 0;
  virtual void AppMenuAnimationEnded() = 0;

  // Schedules a redraw of the icon.
  virtual void InvalidateIcon() = 0;
};

// This class is used for animating and drawing the app menu icon.
class AppMenuAnimation : public gfx::AnimationDelegate {
 public:
  AppMenuAnimation(AppMenuAnimationDelegate* owner, SkColor initial_color);

  ~AppMenuAnimation() override;

  // Paints the app menu icon.
  void PaintAppMenu(gfx::Canvas* canvas, const gfx::Rect& bounds);

  // Starts the animation if it's not already running.
  void StartAnimation();

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  void set_target_color(SkColor target_color) { target_color_ = target_color; }

 private:
  // This class is used to represent and paint a dot on the app menu.
  class AppMenuDot {
   public:
    AppMenuDot(base::TimeDelta delay,
               float width_open_interval,
               float stroke_open_interval);

    // Paints the dot on the given |canvas| according to the progress of
    // |animation|. The size of the dot is calculated to fit in |bounds|.
    // |center_point| is the dot's position on the canvas. The dot's color is
    // a transition from |start_color| to |final_color|.
    void Paint(const gfx::PointF& center_point,
               SkColor start_color,
               SkColor final_color,
               gfx::Canvas* canvas,
               const gfx::Rect& bounds,
               const gfx::SlideAnimation* animation,
               AppMenuAnimationDelegate* delegate);

   private:
    // The delay before the dot starts animating in ms.
    const base::TimeDelta delay_;

    // The percentage of the overall animation duration it takes to animate the
    // width and stroke to their open state.
    const float width_open_interval_;
    const float stroke_open_interval_;

    DISALLOW_COPY_AND_ASSIGN(AppMenuDot);
  };

  AppMenuAnimationDelegate* const delegate_;

  std::unique_ptr<gfx::SlideAnimation> animation_;

  AppMenuDot bottom_dot_;
  AppMenuDot middle_dot_;
  AppMenuDot top_dot_;

  // The starting color of the dots. The animation is expected to transition
  // from this color to |target_color_|.
  SkColor start_color_;

  // The severity color of the dots. This is final color at the end of the
  // animation.
  SkColor target_color_;

  DISALLOW_COPY_AND_ASSIGN(AppMenuAnimation);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ANIMATION_H_
