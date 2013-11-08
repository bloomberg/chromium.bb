// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_settings.h"

namespace cc {

SchedulerSettings::SchedulerSettings()
    : deadline_scheduling_enabled(true),
      impl_side_painting(false),
      timeout_and_draw_when_animation_checkerboards(true),
      maximum_number_of_failed_draws_before_draw_is_forced_(3),
      using_synchronous_renderer_compositor(false),
      throttle_frame_production(true),
      switch_to_low_latency_if_possible(false) {}

SchedulerSettings::~SchedulerSettings() {}

}  // namespace cc
