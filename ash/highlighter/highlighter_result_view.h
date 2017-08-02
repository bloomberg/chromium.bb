// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGHLIGHTER_HIGHLIGHTER_RESULT_VIEW_H_
#define ASH_HIGHLIGHTER_HIGHLIGHTER_RESULT_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace base {
class Timer;
}

namespace ui {
class Layer;
}

namespace views {
class Widget;
}

namespace ash {

// HighlighterResultView displays an animated shape that represents
// the result of the selection.
class HighlighterResultView : public views::View {
 public:
  HighlighterResultView(aura::Window* root_window);

  ~HighlighterResultView() override;

  void AnimateInPlace(const gfx::RectF& bounds, SkColor color);
  void AnimateDeflate(const gfx::RectF& bounds);

 private:
  void ScheduleFadeIn(const base::TimeDelta& delay,
                      const base::TimeDelta& duration);
  void FadeIn(const base::TimeDelta& duration);
  void FadeOut();

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::Layer> result_layer_;
  std::unique_ptr<base::Timer> animation_timer_;

  DISALLOW_COPY_AND_ASSIGN(HighlighterResultView);
};

}  // namespace ash

#endif  // ASH_HIGHLIGHTER_HIGHLIGHTER_RESULT_VIEW_H_
