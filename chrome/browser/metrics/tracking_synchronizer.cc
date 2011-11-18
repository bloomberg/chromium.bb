// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tracking_synchronizer.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread.h"
#include "base/tracked_objects.h"
#include "chrome/browser/ui/webui/tracing_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

using base::TimeTicks;
using content::BrowserThread;

namespace chrome_browser_metrics {

// Negative numbers are never used as sequence numbers.  We explicitly pick a
// negative number that is "so negative" that even when we add one (as is done
// when we generated the next sequence number) that it will still be negative.
// We have code that handles wrapping around on an overflow into negative
// territory.
static const int kNeverUsableSequenceNumber = -2;

TrackingSynchronizer::TrackingSynchronizer()
    : last_used_sequence_number_(kNeverUsableSequenceNumber) {
  DCHECK(tracking_synchronizer_ == NULL);
  tracking_synchronizer_ = this;
}

TrackingSynchronizer::~TrackingSynchronizer() {
  // Just in case we have any pending tasks, clear them out.
  while (!outstanding_requests_.empty()) {
    RequestContextMap::iterator it = outstanding_requests_.begin();
    delete it->second;
    outstanding_requests_.erase(it);
  }

  tracking_synchronizer_ = NULL;
}

// static
TrackingSynchronizer* TrackingSynchronizer::CurrentSynchronizer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(tracking_synchronizer_ != NULL);
  return tracking_synchronizer_;
}

// static
void TrackingSynchronizer::FetchTrackingDataAsynchronously(
    const base::WeakPtr<ProfilerUI>& callback_object) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TrackingSynchronizer* current_synchronizer = CurrentSynchronizer();
  if (current_synchronizer == NULL) {
    // System teardown is happening.
    return;
  }

  int sequence_number = current_synchronizer->RegisterAndNotifyAllProcesses(
      callback_object);

  // Post a task that would be called after waiting for wait_time.  This acts
  // as a watchdog, to cancel the requests for non-responsive processes.
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          current_synchronizer,
          &TrackingSynchronizer::ForceTrackingSynchronizationDoneCallback,
          sequence_number),
      60000);
}

// static
void TrackingSynchronizer::SetTrackingStatus(bool enable) {
  // To iterate over all processes, or to send messages to the hosts, we need
  // to be on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (content::RenderProcessHost::iterator it(
          content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    content::RenderProcessHost* render_process_host = it.GetCurrentValue();
    DCHECK(render_process_host);
    // Ignore processes that don't have a connection, such as crashed tabs.
    if (!render_process_host->HasConnection())
      continue;

    render_process_host->Send(new ChromeViewMsg_SetTrackingStatus(enable));
  }
}

// static
void TrackingSynchronizer::IsTrackingEnabled(int process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // To find the process, or to send messages to the hosts, we need to be on the
  // UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &TrackingSynchronizer::SetTrackingStatusInProcess, process_id));
}

// static
void TrackingSynchronizer::SetTrackingStatusInProcess(int process_id) {
  // To find the process, or to send messages to the hosts, we need to be on the
  // UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  bool enable = tracked_objects::ThreadData::tracking_status();

  content::RenderProcessHost* process =
      content::RenderProcessHost::FromID(process_id);
  // Ignore processes that don't have a connection, such as crashed tabs.
  if (!process || !process->HasConnection())
    return;
  process->Send(new ChromeViewMsg_SetTrackingStatus(enable));
}

// static
void TrackingSynchronizer::DeserializeTrackingList(
    int sequence_number,
    const std::string& tracking_data,
    ChildProcessInfo::ProcessType process_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &TrackingSynchronizer::DeserializeTrackingListOnUI,
          sequence_number, tracking_data, process_type));
}

// static
void TrackingSynchronizer::DeserializeTrackingListOnUI(
    int sequence_number,
    const std::string& tracking_data,
    ChildProcessInfo::ProcessType process_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TrackingSynchronizer* current_synchronizer = CurrentSynchronizer();
  if (current_synchronizer == NULL)
    return;

  base::Value* value =
      base::JSONReader().JsonToValue(tracking_data, false, true);
  DCHECK(value->GetType() == base::Value::TYPE_DICTIONARY);
  base::DictionaryValue* dictionary_value =
      static_cast<DictionaryValue*>(value);
  dictionary_value->SetString(
      "process_type", ChildProcessInfo::GetTypeNameInEnglish(process_type));

  current_synchronizer->DecrementPendingProcessesAndSendData(
      sequence_number, dictionary_value);
}

int TrackingSynchronizer::RegisterAndNotifyAllProcesses(
    const base::WeakPtr<ProfilerUI>& callback_object) {
  // To iterate over all processes, or to send messages to the hosts, we need
  // to be on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int sequence_number = GetNextAvailableSequenceNumber();

  // Initialize processes_pending with one because we are going to send
  // browser's ThreadData.
  RequestContext* request = new RequestContext(
      callback_object, sequence_number, 1, TimeTicks::Now());
  outstanding_requests_[sequence_number] = request;

  DCHECK_GT(request->processes_pending_, 0);
  for (content::RenderProcessHost::iterator it(
          content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    content::RenderProcessHost* render_process_host = it.GetCurrentValue();
    DCHECK(render_process_host);
    // Ignore processes that don't have a connection, such as crashed tabs.
    if (!render_process_host->HasConnection())
      continue;

    ++request->processes_pending_;
    if (!render_process_host->Send(
        new ChromeViewMsg_GetRendererTrackedData(sequence_number))) {
      DecrementPendingProcesses(sequence_number);
    }
  }

  // Get the ThreadData for the browser process and send it back.
  base::DictionaryValue* value = tracked_objects::ThreadData::ToValue();
  const std::string process_type =
      ChildProcessInfo::GetTypeNameInEnglish(ChildProcessInfo::BROWSER_PROCESS);
  value->SetString("process_type", process_type);
  value->SetInteger("process_id", base::GetCurrentProcId());
  DCHECK_GT(request->processes_pending_, 0);
  DecrementPendingProcessesAndSendData(sequence_number, value);

  return sequence_number;
}

void TrackingSynchronizer::DecrementPendingProcessesAndSendData(
    int sequence_number,
    base::DictionaryValue* value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RequestContextMap::iterator it =
      outstanding_requests_.find(sequence_number);
  if (it == outstanding_requests_.end()) {
    delete value;
    return;
  }

  RequestContext* request = NULL;
  request = it->second;
  DCHECK(sequence_number == request->sequence_number_);

  if (value && request->callback_object_) {
    // Transfers ownership of |value| to |callback_object_|.
    request->callback_object_->ReceivedData(value);
  } else {
    delete value;
  }

  if (--request->processes_pending_ <= 0)
    ForceTrackingSynchronizationDoneCallback(sequence_number);
}

void TrackingSynchronizer::DecrementPendingProcesses(int sequence_number) {
  DecrementPendingProcessesAndSendData(sequence_number, NULL);
}

void TrackingSynchronizer::ForceTrackingSynchronizationDoneCallback(
    int sequence_number) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int unresponsive_processes;
  RequestContextMap::iterator it =
      outstanding_requests_.find(sequence_number);
  if (it == outstanding_requests_.end())
    return;

  RequestContext* request = it->second;

  DCHECK(sequence_number == request->sequence_number_);

  unresponsive_processes = request->processes_pending_;

  delete it->second;
  outstanding_requests_.erase(it);

  UMA_HISTOGRAM_COUNTS("Tracking.ProcessNotRespondingAsynchronous",
                       unresponsive_processes);
}

int TrackingSynchronizer::GetNextAvailableSequenceNumber() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ++last_used_sequence_number_;

  // Watch out for wrapping to a negative number.
  if (last_used_sequence_number_ < 0)
    last_used_sequence_number_ = 1;
  return last_used_sequence_number_;
}

// static
TrackingSynchronizer* TrackingSynchronizer::tracking_synchronizer_ = NULL;

}  // namespace chrome_browser_metrics
