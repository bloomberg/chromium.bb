// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_PARAMS_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_PARAMS_H_

#include "base/time/time.h"

namespace metrics {

// Parameters to pass back to the metrics provider.
struct CallStackProfileParams {
  // The process in which the collection occurred.
  enum Process {
    UNKNOWN_PROCESS,
    BROWSER_PROCESS,
    RENDERER_PROCESS,
    GPU_PROCESS,
    UTILITY_PROCESS,
    ZYGOTE_PROCESS,
    SANDBOX_HELPER_PROCESS,
    PPAPI_PLUGIN_PROCESS,
    PPAPI_BROKER_PROCESS
  };

  // The thread from which the collection occurred.
  enum Thread {
    UNKNOWN_THREAD,

    // Browser process threads, some of which occur in other processes as well.
    UI_THREAD,
    FILE_THREAD,
    FILE_USER_BLOCKING_THREAD,
    PROCESS_LAUNCHER_THREAD,
    CACHE_THREAD,
    IO_THREAD,
    DB_THREAD,

    // GPU process thread.
    GPU_MAIN_THREAD,

    // Renderer process threads.
    RENDER_THREAD,
    UTILITY_THREAD
  };

  // The event that triggered the profile collection.
  enum Trigger {
    UNKNOWN,
    PROCESS_STARTUP,
    JANKY_TASK,
    THREAD_HUNG,
    PERIODIC_COLLECTION,
    TRIGGER_LAST = PERIODIC_COLLECTION
  };

  // Allows the caller to specify whether sample ordering is
  // important. MAY_SHUFFLE should always be used to enable better compression,
  // unless the use case needs order to be preserved for a specific reason.
  enum SampleOrderingSpec {
    // The provider may shuffle the sample order to improve compression.
    MAY_SHUFFLE,
    // The provider will not change the sample order.
    PRESERVE_ORDER
  };

  // The default constructor is required for mojo and should not be used
  // otherwise. A valid trigger should always be specified.
  constexpr CallStackProfileParams()
      : CallStackProfileParams(UNKNOWN_PROCESS, UNKNOWN_THREAD, UNKNOWN) {}
  constexpr CallStackProfileParams(Process process,
                                   Thread thread,
                                   Trigger trigger)
      : CallStackProfileParams(process, thread, trigger, MAY_SHUFFLE) {}
  constexpr CallStackProfileParams(Process process,
                                   Thread thread,
                                   Trigger trigger,
                                   SampleOrderingSpec ordering_spec)
      : process(process),
        thread(thread),
        trigger(trigger),
        ordering_spec(ordering_spec) {}

  // The collection process.
  Process process;

  // The collection thread.
  Thread thread;

  // The triggering event.
  Trigger trigger;

  // Whether to preserve sample ordering.
  SampleOrderingSpec ordering_spec;

  // The time at which the CallStackProfileMetricsProvider became aware of the
  // request for profiling. In particular, this is when callback was requested
  // via CallStackProfileMetricsProvider::GetProfilerCallback(). Used to
  // determine if collection was disabled during the collection of the profile.
  base::TimeTicks start_timestamp;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_PARAMS_H_
