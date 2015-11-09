// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/in_flight_change.h"

#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/window_tree_connection.h"

namespace mus {

InFlightChange::InFlightChange(Id window_id, ChangeType type)
    : window_id_(window_id), change_type_(type) {}

InFlightChange::~InFlightChange() {}

InFlightBoundsChange::InFlightBoundsChange(WindowTreeConnection* tree,
                                           Id window_id,
                                           const gfx::Rect& revert_bounds)
    : InFlightChange(window_id, ChangeType::BOUNDS),
      tree_(tree),
      revert_bounds_(revert_bounds) {}

void InFlightBoundsChange::PreviousChangeCompleted(InFlightChange* change,
                                                   bool success) {
  if (!success)
    revert_bounds_ = static_cast<InFlightBoundsChange*>(change)->revert_bounds_;
}

void InFlightBoundsChange::Revert() {
  Window* window = tree_->GetWindowById(window_id());
  if (window)
    WindowPrivate(window).LocalSetBounds(window->bounds(), revert_bounds_);
}

}  // namespace mus
