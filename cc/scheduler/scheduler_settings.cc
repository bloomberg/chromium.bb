// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_settings.h"

#include "base/debug/trace_event_argument.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

SchedulerSettings::SchedulerSettings()
    : begin_frame_scheduling_enabled(true),
      main_frame_before_activation_enabled(false),
      impl_side_painting(false),
      timeout_and_draw_when_animation_checkerboards(true),
      maximum_number_of_failed_draws_before_draw_is_forced_(3),
      using_synchronous_renderer_compositor(false),
      throttle_frame_production(true),
      disable_hi_res_timer_tasks_on_battery(false) {
}

SchedulerSettings::SchedulerSettings(const LayerTreeSettings& settings)
    : begin_frame_scheduling_enabled(settings.begin_frame_scheduling_enabled),
      main_frame_before_activation_enabled(
          settings.main_frame_before_activation_enabled),
      impl_side_painting(settings.impl_side_painting),
      timeout_and_draw_when_animation_checkerboards(
          settings.timeout_and_draw_when_animation_checkerboards),
      maximum_number_of_failed_draws_before_draw_is_forced_(
          settings.maximum_number_of_failed_draws_before_draw_is_forced_),
      using_synchronous_renderer_compositor(
          settings.using_synchronous_renderer_compositor),
      throttle_frame_production(settings.throttle_frame_production),
      disable_hi_res_timer_tasks_on_battery(
          settings.disable_hi_res_timer_tasks_on_battery) {
}

SchedulerSettings::~SchedulerSettings() {}

scoped_refptr<base::debug::ConvertableToTraceFormat>
SchedulerSettings::AsValue() const {
  scoped_refptr<base::debug::TracedValue> state =
      new base::debug::TracedValue();
  state->SetBoolean("begin_frame_scheduling_enabled",
                    begin_frame_scheduling_enabled);
  state->SetBoolean("main_frame_before_activation_enabled",
                    main_frame_before_activation_enabled);
  state->SetBoolean("impl_side_painting", impl_side_painting);
  state->SetBoolean("timeout_and_draw_when_animation_checkerboards",
                    timeout_and_draw_when_animation_checkerboards);
  state->SetInteger("maximum_number_of_failed_draws_before_draw_is_forced_",
                    maximum_number_of_failed_draws_before_draw_is_forced_);
  state->SetBoolean("using_synchronous_renderer_compositor",
                    using_synchronous_renderer_compositor);
  state->SetBoolean("throttle_frame_production", throttle_frame_production);
  state->SetBoolean("disable_hi_res_timer_tasks_on_battery",
                    disable_hi_res_timer_tasks_on_battery);
  return state;
}

}  // namespace cc
