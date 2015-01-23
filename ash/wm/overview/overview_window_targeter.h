// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_WINDOW_TARGETER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_WINDOW_TARGETER_H_

#include "ui/aura/window_targeter.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// A targeter for the windows in the overview. Avoids propagating events to
// window children and uses the WindowSelectorItem bounds to check for hits.
class OverviewWindowTargeter : public aura::WindowTargeter {
 public:
  explicit OverviewWindowTargeter(aura::Window* target);

  ~OverviewWindowTargeter() override;

  // Sets the targetable bounds.
  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds; }

  // ui::EventTargeter:
  ui::EventTarget* FindTargetForLocatedEvent(ui::EventTarget* target,
                                             ui::LocatedEvent* event) override;

 protected:
  // ui::EventTargeter:
  bool EventLocationInsideBounds(ui::EventTarget* target,
                                 const ui::LocatedEvent& event) const override;

 private:
  // Bounds used to check for event hits in the root window coordinates.
  gfx::Rect bounds_;

  // Target to which events are redirected.
  aura::Window* target_;

  DISALLOW_COPY_AND_ASSIGN(OverviewWindowTargeter);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_WINDOW_TARGETER_H_
