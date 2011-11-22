// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_HISTOGRAM_SYNCHRONIZER_H_
#define CHROME_BROWSER_METRICS_HISTOGRAM_SYNCHRONIZER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/time.h"

class MessageLoop;

// This class maintains state that is used to upload histogram data from the
// various renderer processes, into the browser process.  Such transactions are
// usually instigated by the browser.  In general, a renderer process will
// respond by gathering snapshots of all internal histograms, calculating what
// has changed since its last upload, and transmitting a pickled collection of
// deltas.
//
// There are actually two modes of update request.  One is synchronous (and
// blocks the UI thread, waiting to populate an about:histograms tab) and the
// other is asynchronous, and used by the metrics services in preparation for a
// log upload.
//
// To assure that all the renderers have responded, a counter is maintained (for
// each mode) to indicate the number of pending (not yet responsive) renderers.
// To avoid confusion about a response (i.e., is the renderer responding to a
// current request for an update, or to an old request for an update) we tag
// each group of requests with a sequence number.  When an update arrives we can
// ignore it (relative to the counter) if it does not relate to a current
// outstanding sequence number.
//
// There is one final mode of use, where a renderer spontaneously decides to
// transmit a collection of histogram data.  This is designed for use when the
// renderer is terminating.  Unfortunately, renders may be terminated without
// warning, and the best we can do is periodically acquire data from a tab, such
// as when a page load has completed.  In this mode, the renderer uses a
// reserved sequence number, different from any sequence number that might be
// specified by a browser request.  Since this sequence number can't match an
// outstanding sequence number, the pickled data is accepted into the browser,
// but there is no impact on the counters.

class HistogramSynchronizer : public
    base::RefCountedThreadSafe<HistogramSynchronizer> {
 public:

  enum RendererHistogramRequester {
    ASYNC_HISTOGRAMS,
    SYNCHRONOUS_HISTOGRAMS
  };

  // Construction also sets up the global singleton instance.  This instance is
  // used to communicate between the IO and UI thread, and is destroyed only
  // as the main thread (browser_main) terminates, which means the IO thread has
  // already completed, and will not need this instance any further.
  HistogramSynchronizer();

  // Return pointer to the singleton instance, which is allocated and
  // deallocated on the main UI thread (during system startup and teardown).
  static HistogramSynchronizer* CurrentSynchronizer();

  // Contact all renderers, and get them to upload to the browser any/all
  // changes to histograms.  Return when all changes have been acquired, or when
  // the wait time expires (whichever is sooner). This method is called on the
  // main UI thread from about:histograms.
  void FetchRendererHistogramsSynchronously(base::TimeDelta wait_time);

  // Contact all renderers, and get them to upload to the browser any/all
  // changes to histograms.  When all changes have been acquired, or when the
  // wait time expires (whichever is sooner), post the callback to the
  // specified message loop. Note the callback is posted exactly once.
  static void FetchRendererHistogramsAsynchronously(
      MessageLoop* callback_thread,
      const base::Closure& callback,
      int wait_time);

  // This method is called on the IO thread. Deserializes the histograms and
  // records that we have received histograms from a renderer process.
  static void DeserializeHistogramList(
      int sequence_number, const std::vector<std::string>& histograms);

 private:
  friend class base::RefCountedThreadSafe<HistogramSynchronizer>;

  ~HistogramSynchronizer();

  // Establish a new sequence_number_, and use it to notify all the renderers of
  // the need to supply, to the browser, any changes in their histograms.
  // The argument indicates whether this will set async_sequence_number_ or
  // synchronous_sequence_number_.
  // Return the sequence number that was used.
  int NotifyAllRenderers(RendererHistogramRequester requester);

  // Records that we are waiting for one less histogram from a renderer for the
  // given sequence number. If we have received a response from all renderers,
  // either signal the waiting process or call the callback function.
  void DecrementPendingRenderers(int sequence_number);

  // Set the callback_thread_ and callback_ members. If these members already
  // had values, then as a side effect, post the old callback_ to the old
  // callaback_thread_.  This side effect should not generally happen, but is in
  // place to assure correctness (that any tasks that were set, are eventually
  // called, and never merely discarded).
  void SetCallbackTaskAndThread(MessageLoop* callback_thread,
                                const base::Closure& callback);

  void ForceHistogramSynchronizationDoneCallback(int sequence_number);

  // Gets a new sequence number to be sent to renderers from browser process and
  // set the number of pending responses for the given type to renderer_count.
  int GetNextAvailableSequenceNumber(RendererHistogramRequester requster,
                                     int renderer_count);

  // Internal helper function, to post task, and record callback stats.
  void InternalPostTask(MessageLoop* thread,
                        const base::Closure& callback,
                        int unresponsive_renderers,
                        const base::TimeTicks& started);

  // This lock_ protects access to all members.
  base::Lock lock_;

  // This condition variable is used to block caller of the synchronous request
  // to update histograms, and to signal that thread when updates are completed.
  base::ConditionVariable received_all_renderer_histograms_;

  // When a request is made to asynchronously update the histograms, we store
  // the task and thread we use to post a completion notification in
  // callback_ and callback_thread_.
  base::Closure callback_;
  MessageLoop* callback_thread_;

  // We don't track the actual renderers that are contacted for an update, only
  // the count of the number of renderers, and we can sometimes time-out and
  // give up on a "slow to respond" renderer.  We use a sequence_number to be
  // sure a response from a renderer is associated with the current round of
  // requests (and not merely a VERY belated prior response).
  // All sequence numbers used are non-negative.
  // last_used_sequence_number_ is the most recently used number (used to avoid
  // reuse for a long time).
  int last_used_sequence_number_;

  // The sequence number used by the most recent asynchronous update request to
  // contact all renderers.
  int async_sequence_number_;

  // The number of renderers that have not yet responded to requests (as part of
  // an asynchronous update).
  int async_renderers_pending_;

  // The time when we were told to start the fetch histograms asynchronously
  // from renderers.
  base::TimeTicks async_callback_start_time_;

  // The sequence number used by the most recent synchronous update request to
  // contact all renderers.
  int synchronous_sequence_number_;

  // The number of renderers that have not yet responded to requests (as part of
  // a synchronous update).
  int synchronous_renderers_pending_;

  // This singleton instance should be started during the single threaded
  // portion of main(). It initializes globals to provide support for all future
  // calls. This object is created on the UI thread, and it is destroyed after
  // all the other threads have gone away. As a result, it is ok to call it
  // from the UI thread (for UMA uploads), or for about:histograms.
  static HistogramSynchronizer* histogram_synchronizer_;

  DISALLOW_COPY_AND_ASSIGN(HistogramSynchronizer);
};

#endif  // CHROME_BROWSER_METRICS_HISTOGRAM_SYNCHRONIZER_H_
