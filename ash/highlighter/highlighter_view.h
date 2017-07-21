// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGHLIGHTER_HIGHLIGHTER_VIEW_H_
#define ASH_HIGHLIGHTER_HIGHLIGHTER_VIEW_H_

#include <vector>

#include "ash/fast_ink/fast_ink_view.h"
#include "base/time/time.h"

namespace aura {
class Window;
}

namespace base {
class Timer;
}

namespace gfx {
class PointF;
}

namespace ash {

// HighlighterView displays the highlighter palette tool. It draws the
// highlighter stroke which consists of a series of thick lines connecting
// touch points.
class HighlighterView : public FastInkView {
 public:
  enum class AnimationMode {
    kFadeout,
    kInflate,
    kDeflate,
  };

  static const SkColor kPenColor;
  static const gfx::SizeF kPenTipSize;

  explicit HighlighterView(aura::Window* root_window);
  ~HighlighterView() override;

  const std::vector<gfx::PointF>& points() const { return points_; }

  void AddNewPoint(const gfx::PointF& new_point);

  void Animate(const gfx::PointF& pivot, AnimationMode animation_mode);

 private:
  void OnRedraw(gfx::Canvas& canvas, const gfx::Vector2d& offset) override;

  void FadeOut(const gfx::PointF& pivot, AnimationMode animation_mode);

  std::vector<gfx::PointF> points_;
  std::unique_ptr<base::Timer> animation_timer_;

  DISALLOW_COPY_AND_ASSIGN(HighlighterView);
};

}  // namespace ash

#endif  // ASH_HIGHLIGHTER_HIGHLIGHTER_VIEW_H_
