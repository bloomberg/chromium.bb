// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_OBSERVER_H_
#define COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_OBSERVER_H_

#include <vector>

#include "base/process/process_handle.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"
#include "content/public/common/process_type.h"

namespace base {
class TimeDelta;
}

namespace tracked_objects {
struct ProcessDataPhaseSnapshot;
}

namespace metrics {

// Set of profiling events, in no guaranteed order. Implemented as a vector
// because we don't need to have an efficient .find() on it, so vector<> is more
// efficient.
typedef std::vector<ProfilerEventProto::ProfilerEvent> ProfilerEvents;

// Observer for notifications from the TrackingSynchronizer class.
class TrackingSynchronizerObserver {
 public:
  // TODO(vadimt): Consider isherman@ idea: I'd change the phase_start and
  // phase_finish from TimeDeltas to TimeTicks.  And I'd omit the |past_events|
  // list -- either in favor of a single ProfilerEvent that corresponds to the
  // phase, or a method on the TrackingSynchronizer that can translate a
  // profiling_phase to a ProfilerEvent.

  // Received |process_data_phase| for profiling phase |profiling_phase| from a
  // single process of |process_type|. The phase start and finish times,
  // relative to the start time are |phase_start| and
  // |phase_finish|. All profiling phases prior to the reported one have already
  // completed, and each completion was associated with an instance of
  // ProfilerEventProto::ProfilerEvent. |past_events| contains events associated
  // with completions of phases prior to the reported one.
  // The observer should assume there might be more data coming until
  // FinishedReceivingData() is called.
  virtual void ReceivedProfilerData(
      const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
      base::ProcessId process_id,
      content::ProcessType process_type,
      int profiling_phase,
      base::TimeDelta phase_start,
      base::TimeDelta phase_finish,
      const ProfilerEvents& past_events) = 0;

  // The observer should not expect any more calls to |ReceivedProfilerData()|
  // (without re-registering).  This is sent either when data from all processes
  // has been gathered, or when the request times out.
  virtual void FinishedReceivingProfilerData() {}

 protected:
  TrackingSynchronizerObserver() {}
  virtual ~TrackingSynchronizerObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TrackingSynchronizerObserver);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_OBSERVER_H_
