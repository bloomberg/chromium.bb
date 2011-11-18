// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/histogram_synchronizer.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using content::BrowserThread;

// Negative numbers are never used as sequence numbers.  We explicitly pick a
// negative number that is "so negative" that even when we add one (as is done
// when we generated the next sequence number) that it will still be negative.
// We have code that handles wrapping around on an overflow into negative
// territory.
static const int kNeverUsableSequenceNumber = -2;

HistogramSynchronizer::HistogramSynchronizer()
  : lock_(),
    received_all_renderer_histograms_(&lock_),
    callback_task_(NULL),
    callback_thread_(NULL),
    last_used_sequence_number_(kNeverUsableSequenceNumber),
    async_sequence_number_(kNeverUsableSequenceNumber),
    async_renderers_pending_(0),
    synchronous_sequence_number_(kNeverUsableSequenceNumber),
    synchronous_renderers_pending_(0) {
  DCHECK(histogram_synchronizer_ == NULL);
  histogram_synchronizer_ = this;
}

HistogramSynchronizer::~HistogramSynchronizer() {
  // Just in case we have any pending tasks, clear them out.
  SetCallbackTaskAndThread(NULL, NULL);
  histogram_synchronizer_ = NULL;
}

// static
HistogramSynchronizer* HistogramSynchronizer::CurrentSynchronizer() {
  DCHECK(histogram_synchronizer_ != NULL);
  return histogram_synchronizer_;
}

void HistogramSynchronizer::FetchRendererHistogramsSynchronously(
    TimeDelta wait_time) {
  NotifyAllRenderers(SYNCHRONOUS_HISTOGRAMS);

  TimeTicks start = TimeTicks::Now();
  TimeTicks end_time = start + wait_time;
  int unresponsive_renderer_count;
  {
    base::AutoLock auto_lock(lock_);
    while (synchronous_renderers_pending_ > 0 && TimeTicks::Now() < end_time) {
      wait_time = end_time - TimeTicks::Now();
      received_all_renderer_histograms_.TimedWait(wait_time);
    }
    unresponsive_renderer_count = synchronous_renderers_pending_;
    synchronous_renderers_pending_ = 0;
    synchronous_sequence_number_ = kNeverUsableSequenceNumber;
  }
  UMA_HISTOGRAM_COUNTS("Histogram.RendersNotRespondingSynchronous",
                       unresponsive_renderer_count);
  if (!unresponsive_renderer_count)
    UMA_HISTOGRAM_TIMES("Histogram.FetchRendererHistogramsSynchronously",
                        TimeTicks::Now() - start);
}

// static
void HistogramSynchronizer::FetchRendererHistogramsAsynchronously(
    MessageLoop* callback_thread,
    Task* callback_task,
    int wait_time) {
  DCHECK(callback_thread != NULL);
  DCHECK(callback_task != NULL);

  HistogramSynchronizer* current_synchronizer = CurrentSynchronizer();

  if (current_synchronizer == NULL) {
    // System teardown is happening.
    callback_thread->PostTask(FROM_HERE, callback_task);
    return;
  }

  current_synchronizer->SetCallbackTaskAndThread(callback_thread,
                                                 callback_task);

  int sequence_number =
      current_synchronizer->NotifyAllRenderers(ASYNC_HISTOGRAMS);

  // Post a task that would be called after waiting for wait_time.  This acts
  // as a watchdog, to ensure that a non-responsive renderer won't block us from
  // making the callback.
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          current_synchronizer,
          &HistogramSynchronizer::ForceHistogramSynchronizationDoneCallback,
          sequence_number),
      wait_time);
}

// static
void HistogramSynchronizer::DeserializeHistogramList(
    int sequence_number,
    const std::vector<std::string>& histograms) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (std::vector<std::string>::const_iterator it = histograms.begin();
       it < histograms.end();
       ++it) {
    base::Histogram::DeserializeHistogramInfo(*it);
  }

  HistogramSynchronizer* current_synchronizer = CurrentSynchronizer();
  if (current_synchronizer == NULL)
    return;

  // Record that we have received a histogram from renderer process.
  current_synchronizer->DecrementPendingRenderers(sequence_number);
}

int HistogramSynchronizer::NotifyAllRenderers(
    RendererHistogramRequester requester) {
  // To iterate over RenderProcessHosts, or to send messages to the hosts, we
  // need to be on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int notification_count = 0;
  for (content::RenderProcessHost::iterator it(
          content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance())
     ++notification_count;

  int sequence_number = GetNextAvailableSequenceNumber(requester,
                                                       notification_count);
  for (content::RenderProcessHost::iterator it(
          content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    if (!it.GetCurrentValue()->Send(
        new ChromeViewMsg_GetRendererHistograms(sequence_number)))
      DecrementPendingRenderers(sequence_number);
  }

  return sequence_number;
}

void HistogramSynchronizer::DecrementPendingRenderers(int sequence_number) {
  bool synchronous_completed = false;
  bool asynchronous_completed = false;

  {
    base::AutoLock auto_lock(lock_);
    if (sequence_number == async_sequence_number_) {
      if (--async_renderers_pending_ <= 0)
        asynchronous_completed = true;
    } else if (sequence_number == synchronous_sequence_number_) {
      if (--synchronous_renderers_pending_ <= 0)
        synchronous_completed = true;
    }
  }

  if (asynchronous_completed)
    ForceHistogramSynchronizationDoneCallback(sequence_number);
  else if (synchronous_completed)
    received_all_renderer_histograms_.Signal();
}

void HistogramSynchronizer::SetCallbackTaskAndThread(
    MessageLoop* callback_thread,
    Task* callback_task) {
  Task* old_task = NULL;
  MessageLoop* old_thread = NULL;
  TimeTicks old_start_time;
  int unresponsive_renderers;
  const TimeTicks now = TimeTicks::Now();
  {
    base::AutoLock auto_lock(lock_);
    old_task = callback_task_;
    callback_task_ = callback_task;
    old_thread = callback_thread_;
    callback_thread_ = callback_thread;
    unresponsive_renderers = async_renderers_pending_;
    old_start_time = async_callback_start_time_;
    async_callback_start_time_ = now;
    // Prevent premature calling of our new callbacks.
    async_sequence_number_ = kNeverUsableSequenceNumber;
  }
  // Just in case there was a task pending....
  InternalPostTask(old_thread, old_task, unresponsive_renderers,
                   old_start_time);
}

void HistogramSynchronizer::ForceHistogramSynchronizationDoneCallback(
    int sequence_number) {
  Task* task = NULL;
  MessageLoop* thread = NULL;
  TimeTicks started;
  int unresponsive_renderers;
  {
    base::AutoLock lock(lock_);
    if (sequence_number != async_sequence_number_)
      return;
    task = callback_task_;
    thread = callback_thread_;
    callback_task_ = NULL;
    callback_thread_ = NULL;
    started = async_callback_start_time_;
    unresponsive_renderers = async_renderers_pending_;
  }
  InternalPostTask(thread, task, unresponsive_renderers, started);
}

void HistogramSynchronizer::InternalPostTask(MessageLoop* thread, Task* task,
                                             int unresponsive_renderers,
                                             const base::TimeTicks& started) {
  if (!task || !thread)
    return;
  UMA_HISTOGRAM_COUNTS("Histogram.RendersNotRespondingAsynchronous",
                       unresponsive_renderers);
  if (!unresponsive_renderers) {
    UMA_HISTOGRAM_TIMES("Histogram.FetchRendererHistogramsAsynchronously",
                        TimeTicks::Now() - started);
  }

  thread->PostTask(FROM_HERE, task);
}

int HistogramSynchronizer::GetNextAvailableSequenceNumber(
    RendererHistogramRequester requester,
    int renderer_count) {
  base::AutoLock auto_lock(lock_);
  ++last_used_sequence_number_;
  // Watch out for wrapping to a negative number.
  if (last_used_sequence_number_ < 0) {
    // Bypass the reserved number, which is used when a renderer spontaneously
    // decides to send some histogram data.
    last_used_sequence_number_ =
        chrome::kHistogramSynchronizerReservedSequenceNumber + 1;
  }
  DCHECK_NE(last_used_sequence_number_,
            chrome::kHistogramSynchronizerReservedSequenceNumber);
  if (requester == ASYNC_HISTOGRAMS) {
    async_sequence_number_ = last_used_sequence_number_;
    async_renderers_pending_ = renderer_count;
  } else if (requester == SYNCHRONOUS_HISTOGRAMS) {
    synchronous_sequence_number_ = last_used_sequence_number_;
    synchronous_renderers_pending_ = renderer_count;
  }
  return last_used_sequence_number_;
}

// static
HistogramSynchronizer* HistogramSynchronizer::histogram_synchronizer_ = NULL;
