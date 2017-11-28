// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/projecting_observer_chromeos.h"

#include "base/logging.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/display/types/display_snapshot.h"

namespace ash {

ProjectingObserver::ProjectingObserver(
    chromeos::PowerManagerClient* power_manager_client)
    : has_internal_output_(false),
      output_count_(0),
      casting_session_count_(0),
      power_manager_client_(power_manager_client) {
  DCHECK(power_manager_client);
}

ProjectingObserver::~ProjectingObserver() = default;

void ProjectingObserver::OnDisplayModeChanged(
    const display::DisplayConfigurator::DisplayStateList& display_states) {
  has_internal_output_ = false;
  output_count_ = display_states.size();

  for (size_t i = 0; i < display_states.size(); ++i) {
    if (display_states[i]->type() ==
        display::DISPLAY_CONNECTION_TYPE_INTERNAL) {
      has_internal_output_ = true;
      break;
    }
  }

  SetIsProjecting();
}

void ProjectingObserver::OnCastingSessionStartedOrStopped(bool started) {
  if (started) {
    ++casting_session_count_;
  } else {
    DCHECK_GT(casting_session_count_, 0);
    --casting_session_count_;
    if (casting_session_count_ < 0)
      casting_session_count_ = 0;
  }

  SetIsProjecting();
}

void ProjectingObserver::SetIsProjecting() {
  // "Projecting" is defined as having more than 1 output connected while at
  // least one of them is an internal output.
  bool projecting =
      has_internal_output_ && (output_count_ + casting_session_count_ > 1);

  power_manager_client_->SetIsProjecting(projecting);
}

}  // namespace ash
