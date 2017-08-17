// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGHLIGHTER_HIGHLIGHTER_VIEW_H_
#define ASH_HIGHLIGHTER_HIGHLIGHTER_VIEW_H_

#include <vector>

#include "ash/fast_ink/fast_ink_points.h"
#include "ash/fast_ink/fast_ink_view.h"
#include "base/time/time.h"

namespace aura {
class Window;
}

namespace gfx {
class PointF;
}

namespace ash {

enum class HighlighterGestureType;

// HighlighterView displays the highlighter palette tool. It draws the
// highlighter stroke which consists of a series of thick lines connecting
// touch points.
class HighlighterView : public FastInkView {
 public:
  static const SkColor kPenColor;
  static const gfx::SizeF kPenTipSize;

  HighlighterView(const base::TimeDelta presentation_delay,
                  aura::Window* root_window);
  ~HighlighterView() override;

  const FastInkPoints& points() const { return points_; }

  void AddNewPoint(const gfx::PointF& new_point, const base::TimeTicks& time);

  void Animate(const gfx::PointF& pivot,
               HighlighterGestureType gesture_type,
               const base::Closure& done);

 private:
  friend class HighlighterControllerTestApi;

  void OnRedraw(gfx::Canvas& canvas) override;

  void FadeOut(const gfx::PointF& pivot,
               HighlighterGestureType gesture_type,
               const base::Closure& done);

  FastInkPoints points_;
  FastInkPoints predicted_points_;
  const base::TimeDelta presentation_delay_;

  std::unique_ptr<base::OneShotTimer> animation_timer_;

  DISALLOW_COPY_AND_ASSIGN(HighlighterView);
};

}  // namespace ash

#endif  // ASH_HIGHLIGHTER_HIGHLIGHTER_VIEW_H_
