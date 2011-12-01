// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TRACKING_SYNCHRONIZER_H_
#define CHROME_BROWSER_METRICS_TRACKING_SYNCHRONIZER_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/profiler_ui.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"

// This class maintains state that is used to upload tracking data from the
// various processes, into the browser process. Such transactions are usually
// instigated by the browser. In general, a process will respond by gathering
// tracking data, and transmitting the pickled tracking data. We collect the
// data in asynchronous mode that doesn't block the UI thread.
//
// To assure that all the processes have responded, a counter is maintained
// to indicate the number of pending (not yet responsive) processes. We tag
// each group of requests with a sequence number. For each group of requests, we
// create RequestContext object which stores the sequence number, pending
// processes and the callback_object that needs to be notified when we receive
// an update from processes. When an update arrives we find the RequestContext
// associated with sequence number and send the unpickled tracking data to the
// |callback_object_|.

namespace chrome_browser_metrics {

class TrackingSynchronizer : public
    base::RefCountedThreadSafe<TrackingSynchronizer> {
 public:
  // The "RequestContext" structure describes an individual request received
  // from the UI.
  struct RequestContext {
    RequestContext(const base::WeakPtr<ProfilerUI>& callback_object,
                   int sequence_number,
                   int processes_pending,
                   base::TimeTicks callback_start_time)
        : callback_object_(callback_object),
          sequence_number_(sequence_number),
          processes_pending_(processes_pending),
          request_start_time_(callback_start_time) {
    }

    ~RequestContext() {}

    // Requests are made to asynchronously send data to the |callback_object_|.
    base::WeakPtr<ProfilerUI> callback_object_;

    // The sequence number used by the most recent update request to contact all
    // processes.
    int sequence_number_;

    // The number of processes that have not yet responded to requests.
    int processes_pending_;

    // The time when we were told to start the fetching of data from processes.
    base::TimeTicks request_start_time_;
  };

  // A map from sequence_number_ to the actual RequestContexts.
  typedef std::map<int, RequestContext*> RequestContextMap;

  // Construction also sets up the global singleton instance. This instance is
  // used to communicate between the IO and UI thread, and is destroyed only as
  // the main thread (browser_main) terminates, which means the IO thread has
  // already completed, and will not need this instance any further.
  TrackingSynchronizer();

  // Return pointer to the singleton instance, which is allocated and
  // deallocated on the main UI thread (during system startup and teardown).
  static TrackingSynchronizer* CurrentSynchronizer();

  // Contact all processes, and get them to upload to the browser any/all
  // changes to tracking data. It calls |callback_object|'s SetData method with
  // the data received from each sub-process.
  // This method is accessible on UI thread.
  static void FetchTrackingDataAsynchronously(
      const base::WeakPtr<ProfilerUI>& callback_object);

  // Contact all processes and set tracking status to |enable|.
  // This method is accessible on UI thread.
  static void SetTrackingStatus(bool enable);

  // Respond to this message from the renderer by setting the tracking status
  // (SetTrackingStatusInProcess) in that renderer process.
  // |process_id| is used to find the renderer process.
  // This method is accessible on IO thread.
  static void IsTrackingEnabled(int process_id);

  // Get the current tracking status from the browser process and set it in the
  // renderer process. |process_id| is used to find the renderer process.
  // This method is accessible on UI thread.
  static void SetTrackingStatusInProcess(int process_id);

  // Deserialize the tracking data and record that we have received tracking
  // data from a process. This method posts a task to call
  // DeserializeTrackingListOnUI on UI thread to send the |tracking_data| to
  // callback_object_. This method is accessible on IO thread.
  static void DeserializeTrackingList(
      int sequence_number,
      const std::string& tracking_data,
      content::ProcessType process_type);

  // Deserialize the tracking data and record that we have received tracking
  // data from a process. This method is accessible on UI thread.
  static void DeserializeTrackingListOnUI(
      int sequence_number,
      const std::string& tracking_data,
      content::ProcessType process_type);

 private:
  friend class base::RefCountedThreadSafe<TrackingSynchronizer>;

  virtual ~TrackingSynchronizer();

  // Establish a new sequence_number_, and use it to notify all the processes of
  // the need to supply, to the browser, their tracking data. It also registers
  // |callback_object| in |outstanding_requests_| map. Return the
  // sequence_number_ that was used. This method is accessible on UI thread.
  int RegisterAndNotifyAllProcesses(
      const base::WeakPtr<ProfilerUI>& callback_object);

  // It finds the |callback_object_| in |outstanding_requests_| map for the
  // given |sequence_number| and notifies the |callback_object_| about the
  // |value|. This is called whenever we receive tracked data from processes. It
  // also records that we are waiting for one less tracking data from a process
  // for the given sequence number. If we have received a response from all
  // renderers, then it deletes the entry for sequence_number from
  // |outstanding_requests_| map. This method is accessible on UI thread.
  void DecrementPendingProcessesAndSendData(int sequence_number,
                                            base::DictionaryValue* value);

  // Records that we are waiting for one less tracking data from a process for
  // the given sequence number.
  // This method is accessible on UI thread.
  void DecrementPendingProcesses(int sequence_number);

  // When all changes have been acquired, or when the wait time expires
  // (whichever is sooner), this method is called. This method deletes the entry
  // for the given sequence_number from |outstanding_requests_| map.
  // This method is accessible on UI thread.
  void ForceTrackingSynchronizationDoneCallback(int sequence_number);

  // Get a new sequence number to be sent to processes from browser process.
  // This method is accessible on UI thread.
  int GetNextAvailableSequenceNumber();

  // Map of all outstanding RequestContexts, from sequence_number_ to
  // RequestContext.
  RequestContextMap outstanding_requests_;

  // We don't track the actual processes that are contacted for an update, only
  // the count of the number of processes, and we can sometimes time-out and
  // give up on a "slow to respond" process.  We use a sequence_number to be
  // sure a response from a process is associated with the current round of
  // requests. All sequence numbers used are non-negative.
  // last_used_sequence_number_ is the most recently used number (used to avoid
  // reuse for a long time).
  int last_used_sequence_number_;

  // This singleton instance should be started during the single threaded
  // portion of main(). It initializes globals to provide support for all future
  // calls. This object is created on the UI thread, and it is destroyed after
  // all the other threads have gone away. As a result, it is ok to call it
  // from the UI thread, or for about:tracking.
  static TrackingSynchronizer* tracking_synchronizer_;

  DISALLOW_COPY_AND_ASSIGN(TrackingSynchronizer);
};

}  // namespace chrome_browser_metrics

#endif  // CHROME_BROWSER_METRICS_TRACKING_SYNCHRONIZER_H_
