// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/in_flight_change.h"

#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/window_tree_connection.h"

namespace mus {

// InFlightChange -------------------------------------------------------------

InFlightChange::InFlightChange(Window* window, ChangeType type)
    : window_(window), change_type_(type) {}

InFlightChange::~InFlightChange() {}

bool InFlightChange::Matches(const InFlightChange& change) const {
  DCHECK(change.window_ == window_ && change.change_type_ == change_type_);
  return true;
}

void InFlightChange::ChangeFailed() {}

// InFlightBoundsChange -------------------------------------------------------

InFlightBoundsChange::InFlightBoundsChange(Window* window,
                                           const gfx::Rect& revert_bounds)
    : InFlightChange(window, ChangeType::BOUNDS),
      revert_bounds_(revert_bounds) {}

void InFlightBoundsChange::SetRevertValueFrom(const InFlightChange& change) {
  revert_bounds_ =
      static_cast<const InFlightBoundsChange&>(change).revert_bounds_;
}

void InFlightBoundsChange::Revert() {
  WindowPrivate(window()).LocalSetBounds(window()->bounds(), revert_bounds_);
}

// CrashInFlightChange --------------------------------------------------------

CrashInFlightChange::CrashInFlightChange(Window* window, ChangeType type)
    : InFlightChange(window, type) {}

CrashInFlightChange::~CrashInFlightChange() {}

void CrashInFlightChange::SetRevertValueFrom(const InFlightChange& change) {
  CHECK(false);
}

void CrashInFlightChange::ChangeFailed() {
  CHECK(false);
}

void CrashInFlightChange::Revert() {
  CHECK(false);
}

// InFlightPropertyChange -----------------------------------------------------

InFlightPropertyChange::InFlightPropertyChange(
    Window* window,
    const std::string& property_name,
    const mojo::Array<uint8_t>& revert_value)
    : InFlightChange(window, ChangeType::PROPERTY),
      property_name_(property_name),
      revert_value_(revert_value.Clone()) {}

InFlightPropertyChange::~InFlightPropertyChange() {}

bool InFlightPropertyChange::Matches(const InFlightChange& change) const {
  return static_cast<const InFlightPropertyChange&>(change).property_name_ ==
         property_name_;
}

void InFlightPropertyChange::SetRevertValueFrom(const InFlightChange& change) {
  revert_value_ =
      static_cast<const InFlightPropertyChange&>(change).revert_value_.Clone();
}

void InFlightPropertyChange::Revert() {
  WindowPrivate(window())
      .LocalSetSharedProperty(property_name_, revert_value_.Pass());
}

}  // namespace mus
