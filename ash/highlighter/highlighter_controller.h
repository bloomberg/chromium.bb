// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGHLIGHTER_HIGHLIGHTER_CONTROLLER_H_
#define ASH_HIGHLIGHTER_HIGHLIGHTER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

class HighlighterResultView;
class HighlighterSelectionObserver;
class HighlighterView;

// Controller for the highlighter functionality.
// Enables/disables highlighter as well as receives points
// and passes them off to be rendered.
class ASH_EXPORT HighlighterController : public ui::EventHandler,
                                         public aura::WindowObserver {
 public:
  HighlighterController();
  ~HighlighterController() override;

  // Turns the highlighter feature on. The user still has to press and
  // drag to see the highlighter.
  void EnableHighlighter(HighlighterSelectionObserver* observer);

  void DisableHighlighter();

 private:
  friend class HighlighterControllerTestApi;

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Handles monitor disconnect/swap primary.
  void SwitchTargetRootWindowIfNeeded(aura::Window* root_window);

  // Updates |highlighter_view_| by changing its root window or creating it if
  // needed. Also adds the latest point at |event_location| and stops
  // propagation of |event|.
  void UpdateHighlighterView(aura::Window* current_window,
                             const gfx::PointF& event_location,
                             ui::Event* event);

  // Destroys |highlighter_view_|, if it exists.
  void DestroyHighlighterView();

  // The presentation delay used for prediction by the highlighter view.
  base::TimeDelta presentation_delay_;

  // |highlighter_view_| will only hold an instance when the highlighter is
  // enabled and activated (pressed or dragged).
  std::unique_ptr<HighlighterView> highlighter_view_;

  std::unique_ptr<HighlighterResultView> result_view_;

  // |observer_| in non-null when the highlighter is enabled,
  // null when disabled.
  HighlighterSelectionObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(HighlighterController);
};

}  // namespace ash

#endif  // ASH_HIGHLIGHTER_HIGHLIGHTER_CONTROLLER_H_
