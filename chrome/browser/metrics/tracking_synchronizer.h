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
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/profiler_ui.h"
#include "content/public/browser/profiler_subscriber.h"

// This class maintains state that is used to upload profiler data from the
// various processes, into the browser process. Such transactions are usually
// instigated by the browser. In general, a process will respond by gathering
// profiler data, and transmitting the pickled profiler data. We collect the
// data in asynchronous mode that doesn't block the UI thread.
//
// To assure that all the processes have responded, a counter is maintained
// to indicate the number of pending (not yet responsive) processes. We tag
// each group of requests with a sequence number. For each group of requests, we
// create RequestContext object which stores the sequence number, pending
// processes and the callback_object that needs to be notified when we receive
// an update from processes. When an update arrives we find the RequestContext
// associated with sequence number and send the unpickled profiler data to the
// |callback_object_|.

namespace chrome_browser_metrics {

class RequestContext;

class TrackingSynchronizer
    : public content::ProfilerSubscriber,
      public base::RefCountedThreadSafe<TrackingSynchronizer> {
 public:
  // Construction also sets up the global singleton instance. This instance is
  // used to communicate between the IO and UI thread, and is destroyed only as
  // the main thread (browser_main) terminates, which means the IO thread has
  // already completed, and will not need this instance any further.
  TrackingSynchronizer();

  // Contact all processes, and get them to upload to the browser any/all
  // changes to profiler data. It calls |callback_object|'s SetData method with
  // the data received from each sub-process.
  // This method is accessible on UI thread.
  static void FetchProfilerDataAsynchronously(
      const base::WeakPtr<ProfilerUI>& callback_object);

  // ------------------------------------------------------
  // ProfilerSubscriber methods for browser child processes
  // ------------------------------------------------------

  // Update the number of pending processes for the given |sequence_number|.
  // This is called on UI thread.
  virtual void OnPendingProcesses(int sequence_number,
                                  int pending_processes,
                                  bool end) OVERRIDE;

  // Send profiler_data back to callback_object_ by calling
  // DecrementPendingProcessesAndSendData which records that we are waiting
  // for one less profiler data from renderer or browser child process for the
  // given sequence number. This method is accessible on UI thread.
  virtual void OnProfilerDataCollected(
      int sequence_number,
      base::DictionaryValue* profiler_data) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<TrackingSynchronizer>;
  friend class RequestContext;

  virtual ~TrackingSynchronizer();

  // Send profiler_data back to callback_object_. It records that we are waiting
  // for one less profiler data from renderer or browser child process for the
  // given sequence number. This method is accessible on UI thread.
  void OnProfilerDataCollectedOnUI(int sequence_number,
                                   base::DictionaryValue* profiler_data);

  // Establish a new sequence_number_, and use it to notify all the processes of
  // the need to supply, to the browser, their tracking data. It also registers
  // |callback_object| in |outstanding_requests_| map. Return the
  // sequence_number_ that was used. This method is accessible on UI thread.
  int RegisterAndNotifyAllProcesses(
      const base::WeakPtr<ProfilerUI>& callback_object);

  // It finds the RequestContext for the given |sequence_number| and notifies
  // the RequestContext's |callback_object_| about the |value|. This is called
  // whenever we receive profiler data from processes. It also records that we
  // are waiting for one less profiler data from a process for the given
  // sequence number. If we have received a response from all renderers and
  // browser processes, then it calls RequestContext's DeleteIfAllDone to delete
  // the entry for sequence_number. This method is accessible on UI thread.
  void DecrementPendingProcessesAndSendData(int sequence_number,
                                            base::DictionaryValue* value);

  // Get a new sequence number to be sent to processes from browser process.
  // This method is accessible on UI thread.
  int GetNextAvailableSequenceNumber();

  // Return pointer to the singleton instance, which is allocated and
  // deallocated on the main UI thread (during system startup and teardown).
  // This method is accessible on UI thread.
  static TrackingSynchronizer* CurrentSynchronizer();

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
  // from the UI thread, or for about:profiler.
  static TrackingSynchronizer* tracking_synchronizer_;

  DISALLOW_COPY_AND_ASSIGN(TrackingSynchronizer);
};

}  // namespace chrome_browser_metrics

#endif  // CHROME_BROWSER_METRICS_TRACKING_SYNCHRONIZER_H_
