// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_LOGO_VIEW_LOGO_VIEW_H_
#define ASH_ASSISTANT_UI_LOGO_VIEW_LOGO_VIEW_H_

#include <cstdint>
#include <memory>

#include "ash/assistant/ui/logo_view/shape/mic_part_shape.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chromeos/assistant/internal/logo_view/logo_model/logo.h"
#include "chromeos/assistant/internal/logo_view/state_animator.h"
#include "chromeos/assistant/internal/logo_view/state_animator_timer_delegate.h"
#include "chromeos/assistant/internal/logo_view/state_model.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/views/view.h"

namespace chromeos {
namespace assistant {
class Dot;
}  // namespace assistant
}  // namespace chromeos

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ui {
class Compositor;
}

namespace views {
class View;
}

namespace ash {

class Shape;

// Displays the GLIF (Google Logo and Identity Family). It displays the dots and
// the google logo. It does not display the Super G.
class LogoView : public views::View,
                 public chromeos::assistant::StateAnimatorTimerDelegate,
                 public ui::CompositorAnimationObserver {
 public:
  using Dot = chromeos::assistant::Dot;
  using Logo = chromeos::assistant::Logo;
  using StateAnimator = chromeos::assistant::StateAnimator;
  using StateModel = chromeos::assistant::StateModel;

  LogoView();
  ~LogoView() override;

  // chromeos::assistant::StateAnimatorTimerDelegate:
  int64_t StartTimer() override;
  void StopTimer() override;

  // Switch to different LogoView state. If |animate| is false, no
  // exit animation of current state and enter animation of the next state.
  void SwitchStateTo(StateModel::State state, bool animate);

 private:
  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  void DrawDots(gfx::Canvas* canvas);
  void DrawDot(gfx::Canvas* canvas, Dot* dot);
  void DrawMicPart(gfx::Canvas* canvas, Dot* dot, float x, float y);
  void DrawShape(gfx::Canvas* canvas, Shape* shape, SkColor color);
  void DrawLine(gfx::Canvas* canvas, Dot* dot, float x, float y);
  void DrawCircle(gfx::Canvas* canvas, Dot* dot, float x, float y);

  // TODO(b/79579731): Implement the letter animation.
  void DrawLetter(gfx::Canvas* canvas, Dot* dot, float x, float y) {}

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  Logo logo_;

  MicPartShape mic_part_shape_;

  StateModel state_model_;

  StateAnimator state_animator_;

  ui::Compositor* animating_compositor_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LogoView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_LOGO_VIEW_LOGO_VIEW_H_
