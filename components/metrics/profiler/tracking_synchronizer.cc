// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/profiler/tracking_synchronizer.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread.h"
#include "base/tracked_objects.h"
#include "components/metrics/profiler/tracking_synchronizer_observer.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/profiler_controller.h"
#include "content/public/common/process_type.h"

using base::TimeTicks;
using content::BrowserThread;

namespace metrics {

namespace {

// Negative numbers are never used as sequence numbers.  We explicitly pick a
// negative number that is "so negative" that even when we add one (as is done
// when we generated the next sequence number) that it will still be negative.
// We have code that handles wrapping around on an overflow into negative
// territory.
const int kNeverUsableSequenceNumber = -2;

// This singleton instance should be started during the single threaded
// portion of main(). It initializes globals to provide support for all future
// calls. This object is created on the UI thread, and it is destroyed after
// all the other threads have gone away. As a result, it is ok to call it
// from the UI thread, or for about:profiler.
static TrackingSynchronizer* g_tracking_synchronizer = NULL;

}  // namespace

// The "RequestContext" structure describes an individual request received
// from the UI. All methods are accessible on UI thread.
class TrackingSynchronizer::RequestContext {
 public:
  // A map from sequence_number_ to the actual RequestContexts.
  typedef std::map<int, RequestContext*> RequestContextMap;

  RequestContext(
      const base::WeakPtr<TrackingSynchronizerObserver>& callback_object,
      int sequence_number)
      : callback_object_(callback_object),
        sequence_number_(sequence_number),
        received_process_group_count_(0),
        processes_pending_(0) {
  }
  ~RequestContext() {}

  void SetReceivedProcessGroupCount(bool done) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    received_process_group_count_ = done;
  }

  // Methods for book keeping of processes_pending_.
  void IncrementProcessesPending() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ++processes_pending_;
  }

  void AddProcessesPending(int processes_pending) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    processes_pending_ += processes_pending;
  }

  void DecrementProcessesPending() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    --processes_pending_;
  }

  // Records that we are waiting for one less tracking data from a process for
  // the given sequence number. If |received_process_group_count_| and
  // |processes_pending_| are zero, then delete the current object by calling
  // Unregister.
  void DeleteIfAllDone() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (processes_pending_ <= 0 && received_process_group_count_)
      RequestContext::Unregister(sequence_number_);
  }

  // Register |callback_object| in |outstanding_requests_| map for the given
  // |sequence_number|.
  static RequestContext* Register(
      int sequence_number,
      const base::WeakPtr<TrackingSynchronizerObserver>& callback_object) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    RequestContext* request = new RequestContext(
        callback_object, sequence_number);
    outstanding_requests_.Get()[sequence_number] = request;

    return request;
  }

  // Find the |RequestContext| in |outstanding_requests_| map for the given
  // |sequence_number|.
  static RequestContext* GetRequestContext(int sequence_number) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    RequestContextMap::iterator it =
        outstanding_requests_.Get().find(sequence_number);
    if (it == outstanding_requests_.Get().end())
      return NULL;

    RequestContext* request = it->second;
    DCHECK_EQ(sequence_number, request->sequence_number_);
    return request;
  }

  // Delete the entry for the given |sequence_number| from
  // |outstanding_requests_| map. This method is called when all changes have
  // been acquired, or when the wait time expires (whichever is sooner).
  static void Unregister(int sequence_number) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    RequestContextMap::iterator it =
        outstanding_requests_.Get().find(sequence_number);
    if (it == outstanding_requests_.Get().end())
      return;

    RequestContext* request = it->second;
    DCHECK_EQ(sequence_number, request->sequence_number_);
    bool received_process_group_count = request->received_process_group_count_;
    int unresponsive_processes = request->processes_pending_;

    if (request->callback_object_.get())
      request->callback_object_->FinishedReceivingProfilerData();

    delete request;
    outstanding_requests_.Get().erase(it);

    UMA_HISTOGRAM_BOOLEAN("Profiling.ReceivedProcessGroupCount",
                          received_process_group_count);
    UMA_HISTOGRAM_COUNTS("Profiling.PendingProcessNotResponding",
                         unresponsive_processes);
  }

  // Delete all the entries in |outstanding_requests_| map.
  static void OnShutdown() {
    // Just in case we have any pending tasks, clear them out.
    while (!outstanding_requests_.Get().empty()) {
      RequestContextMap::iterator it = outstanding_requests_.Get().begin();
      delete it->second;
      outstanding_requests_.Get().erase(it);
    }
  }

  // Requests are made to asynchronously send data to the |callback_object_|.
  base::WeakPtr<TrackingSynchronizerObserver> callback_object_;

  // The sequence number used by the most recent update request to contact all
  // processes.
  int sequence_number_;

  // Indicates if we have received all pending processes count.
  bool received_process_group_count_;

  // The number of pending processes (browser, all renderer processes and
  // browser child processes) that have not yet responded to requests.
  int processes_pending_;

  // Map of all outstanding RequestContexts, from sequence_number_ to
  // RequestContext.
  static base::LazyInstance<RequestContextMap>::Leaky outstanding_requests_;
};

// static
base::LazyInstance
    <TrackingSynchronizer::RequestContext::RequestContextMap>::Leaky
        TrackingSynchronizer::RequestContext::outstanding_requests_ =
            LAZY_INSTANCE_INITIALIZER;

// TrackingSynchronizer methods and members.

TrackingSynchronizer::TrackingSynchronizer(scoped_ptr<base::TickClock> clock)
    : last_used_sequence_number_(kNeverUsableSequenceNumber),
      clock_(clock.Pass()) {
  DCHECK(!g_tracking_synchronizer);
  g_tracking_synchronizer = this;
  phase_start_times_.push_back(clock_->NowTicks());

#if !defined(OS_IOS)
  // TODO: This ifdef and other ifdefs for OS_IOS in this file are only
  // short-term hacks to make this compile on iOS, and the proper solution is to
  // refactor to remove content dependencies from shared code.
  // See crbug/472210.
  content::ProfilerController::GetInstance()->Register(this);
#endif
}

TrackingSynchronizer::~TrackingSynchronizer() {
#if !defined(OS_IOS)
  content::ProfilerController::GetInstance()->Unregister(this);
#endif

  // Just in case we have any pending tasks, clear them out.
  RequestContext::OnShutdown();

  g_tracking_synchronizer = NULL;
}

// static
void TrackingSynchronizer::FetchProfilerDataAsynchronously(
    const base::WeakPtr<TrackingSynchronizerObserver>& callback_object) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!g_tracking_synchronizer) {
    // System teardown is happening.
    return;
  }

  int sequence_number = g_tracking_synchronizer->RegisterAndNotifyAllProcesses(
      callback_object);

  // Post a task that would be called after waiting for wait_time.  This acts
  // as a watchdog, to cancel the requests for non-responsive processes.
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RequestContext::Unregister, sequence_number),
      base::TimeDelta::FromMinutes(1));
}

// static
void TrackingSynchronizer::OnProfilingPhaseCompleted(
    ProfilerEventProto::ProfilerEvent profiling_event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!g_tracking_synchronizer) {
    // System teardown is happening.
    return;
  }

  g_tracking_synchronizer->NotifyAllProcessesOfProfilingPhaseCompletion(
      profiling_event);
}

void TrackingSynchronizer::OnPendingProcesses(int sequence_number,
                                              int pending_processes,
                                              bool end) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RequestContext* request = RequestContext::GetRequestContext(sequence_number);
  if (!request)
    return;
  request->AddProcessesPending(pending_processes);
  request->SetReceivedProcessGroupCount(end);
  request->DeleteIfAllDone();
}

void TrackingSynchronizer::OnProfilerDataCollected(
    int sequence_number,
    const tracked_objects::ProcessDataSnapshot& profiler_data,
    content::ProcessType process_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DecrementPendingProcessesAndSendData(sequence_number, profiler_data,
                                       process_type);
}

int TrackingSynchronizer::RegisterAndNotifyAllProcesses(
    const base::WeakPtr<TrackingSynchronizerObserver>& callback_object) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int sequence_number = GetNextAvailableSequenceNumber();

  RequestContext* request =
      RequestContext::Register(sequence_number, callback_object);

  // Increment pending process count for sending browser's profiler data.
  request->IncrementProcessesPending();

  const int current_profiling_phase = phase_completion_events_sequence_.size();

#if !defined(OS_IOS)
  // Get profiler data from renderer and browser child processes.
  content::ProfilerController::GetInstance()->GetProfilerData(
      sequence_number, current_profiling_phase);
#else
  // On non-iOS platforms, |OnPendingProcesses()| is called from
  // |content::ProfilerController|. On iOS, manually call the method to indicate
  // that there is no need to wait for data from child processes. On iOS, there
  // is only the main browser process.
  OnPendingProcesses(sequence_number, 0, true);
#endif  // !defined(OS_IOS)

  // Send process data snapshot from browser process.
  tracked_objects::ProcessDataSnapshot process_data_snapshot;
  tracked_objects::ThreadData::Snapshot(current_profiling_phase,
                                        &process_data_snapshot);

  DecrementPendingProcessesAndSendData(sequence_number, process_data_snapshot,
                                       content::PROCESS_TYPE_BROWSER);

  return sequence_number;
}

void TrackingSynchronizer::RegisterPhaseCompletion(
    ProfilerEventProto::ProfilerEvent profiling_event) {
  phase_completion_events_sequence_.push_back(profiling_event);
  phase_start_times_.push_back(clock_->NowTicks());
}

void TrackingSynchronizer::NotifyAllProcessesOfProfilingPhaseCompletion(
    ProfilerEventProto::ProfilerEvent profiling_event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (variations::GetVariationParamValue("UMALogPhasedProfiling",
                                         "send_split_profiles") == "false") {
    return;
  }

  int profiling_phase = phase_completion_events_sequence_.size();

  // If you hit this check, stop and think. You just added a new profiling
  // phase. Each profiling phase takes additional memory in DeathData's list of
  // snapshots. We cannot grow it indefinitely. Consider collapsing older phases
  // after they were sent to UMA server, or other ways to save memory.
  DCHECK_LT(profiling_phase, 1);

  RegisterPhaseCompletion(profiling_event);

#if !defined(OS_IOS)
  // Notify renderer and browser child processes.
  content::ProfilerController::GetInstance()->OnProfilingPhaseCompleted(
      profiling_phase);
#endif

  // Notify browser process.
  tracked_objects::ThreadData::OnProfilingPhaseCompleted(profiling_phase);
}

void TrackingSynchronizer::SendData(
    const tracked_objects::ProcessDataSnapshot& profiler_data,
    content::ProcessType process_type,
    TrackingSynchronizerObserver* observer) const {
  // We are going to loop though past profiling phases and notify the request
  // about each phase that is contained in profiler_data. past_events
  // will track the set of past profiling events as we go.
  ProfilerEvents past_events;

  // Go through all completed phases, and through the current one. The current
  // one is not in phase_completion_events_sequence_, but note the <=
  // comparison.
  for (size_t phase = 0; phase <= phase_completion_events_sequence_.size();
       ++phase) {
    auto it = profiler_data.phased_snapshots.find(phase);

    if (it != profiler_data.phased_snapshots.end()) {
      // If the phase is contained in the received snapshot, notify the
      // request.
      const base::TimeTicks phase_start = phase_start_times_[phase];
      const base::TimeTicks phase_finish = phase + 1 < phase_start_times_.size()
                                               ? phase_start_times_[phase + 1]
                                               : clock_->NowTicks();
      observer->ReceivedProfilerData(
          ProfilerDataAttributes(phase, profiler_data.process_id, process_type,
                                 phase_start, phase_finish),
          it->second, past_events);
    }

    if (phase < phase_completion_events_sequence_.size()) {
      past_events.push_back(phase_completion_events_sequence_[phase]);
    }
  }
}

void TrackingSynchronizer::DecrementPendingProcessesAndSendData(
    int sequence_number,
    const tracked_objects::ProcessDataSnapshot& profiler_data,
    content::ProcessType process_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RequestContext* request = RequestContext::GetRequestContext(sequence_number);
  if (!request)
    return;

  TrackingSynchronizerObserver* observer = request->callback_object_.get();
  if (observer)
    SendData(profiler_data, process_type, observer);

  // Delete request if we have heard back from all child processes.
  request->DecrementProcessesPending();
  request->DeleteIfAllDone();
}

int TrackingSynchronizer::GetNextAvailableSequenceNumber() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ++last_used_sequence_number_;

  // Watch out for wrapping to a negative number.
  if (last_used_sequence_number_ < 0)
    last_used_sequence_number_ = 1;
  return last_used_sequence_number_;
}

}  // namespace metrics
