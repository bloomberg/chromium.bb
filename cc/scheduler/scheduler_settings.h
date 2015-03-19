// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_SETTINGS_H_
#define CC_SCHEDULER_SCHEDULER_SETTINGS_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/base/cc_export.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {

class CC_EXPORT SchedulerSettings {
 public:
  SchedulerSettings();
  ~SchedulerSettings();

  bool use_external_begin_frame_source;
  bool main_frame_before_activation_enabled;
  bool impl_side_painting;
  bool timeout_and_draw_when_animation_checkerboards;
  int maximum_number_of_failed_draws_before_draw_is_forced_;
  bool using_synchronous_renderer_compositor;
  bool throttle_frame_production;

  // In main thread low latency mode the entire
  // BeginMainFrame->Commit->Activation->Draw cycle should complete before
  // starting the next cycle.  Additionally, BeginMainFrame and Commit are
  // completed atomically with no other tasks or actions occuring between them.
  bool main_thread_should_always_be_low_latency;

  base::TimeDelta background_frame_interval;

  scoped_refptr<base::trace_event::ConvertableToTraceFormat> AsValue() const;
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_SETTINGS_H_
