// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_window_targeter.h"
#include "ui/aura/window.h"

namespace ash {

OverviewWindowTargeter::OverviewWindowTargeter(aura::Window* target)
    : bounds_(gfx::Rect()), target_(target) {
}

OverviewWindowTargeter::~OverviewWindowTargeter() {
}

ui::EventTarget* OverviewWindowTargeter::FindTargetForLocatedEvent(
    ui::EventTarget* target,
    ui::LocatedEvent* event) {
  // Manually override the location of the event to make sure it targets the
  // contents view on the target window.
  event->set_location(gfx::PointF(0, 0));
  return target_;
}

bool OverviewWindowTargeter::EventLocationInsideBounds(
    ui::EventTarget* target,
    const ui::LocatedEvent& event) const {
  return bounds_.Contains(event.location());
}

}  // namespace ash
