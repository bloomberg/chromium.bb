// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_settings.h"

#include "base/trace_event/trace_event_argument.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

SchedulerSettings::SchedulerSettings()
    : use_external_begin_frame_source(false),
      main_frame_before_activation_enabled(false),
      impl_side_painting(false),
      timeout_and_draw_when_animation_checkerboards(true),
      maximum_number_of_failed_draws_before_draw_is_forced_(3),
      using_synchronous_renderer_compositor(false),
      throttle_frame_production(true),
      main_thread_should_always_be_low_latency(false),
      background_frame_interval(base::TimeDelta::FromSeconds(1)) {
}

SchedulerSettings::SchedulerSettings(const LayerTreeSettings& settings)
    : use_external_begin_frame_source(settings.use_external_begin_frame_source),
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
      main_thread_should_always_be_low_latency(false),
      background_frame_interval(base::TimeDelta::FromSecondsD(
          1.0 / settings.background_animation_rate)) {
}

SchedulerSettings::~SchedulerSettings() {}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
SchedulerSettings::AsValue() const {
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();
  state->SetBoolean("use_external_begin_frame_source",
                    use_external_begin_frame_source);
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
  state->SetBoolean("main_thread_should_always_be_low_latency",
                    main_thread_should_always_be_low_latency);
  state->SetInteger("background_frame_interval",
                    background_frame_interval.InMicroseconds());
  return state;
}

}  // namespace cc
