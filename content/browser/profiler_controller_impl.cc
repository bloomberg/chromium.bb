// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/profiler_controller_impl.h"

#include "base/bind.h"
#include "base/process/process_handle.h"
#include "base/tracked_objects.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/profiler_subscriber.h"
#include "content/public/browser/render_process_host.h"

namespace content {

ProfilerController* ProfilerController::GetInstance() {
  return ProfilerControllerImpl::GetInstance();
}

ProfilerControllerImpl* ProfilerControllerImpl::GetInstance() {
  return base::Singleton<ProfilerControllerImpl>::get();
}

ProfilerControllerImpl::ProfilerControllerImpl() : subscriber_(NULL) {
}

ProfilerControllerImpl::~ProfilerControllerImpl() {
}

void ProfilerControllerImpl::OnPendingProcesses(int sequence_number,
                                                int pending_processes,
                                                bool end) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (subscriber_)
    subscriber_->OnPendingProcesses(sequence_number, pending_processes, end);
}

void ProfilerControllerImpl::OnProfilerDataCollected(
    int sequence_number,
    const tracked_objects::ProcessDataSnapshot& profiler_data,
    content::ProcessType process_type) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&ProfilerControllerImpl::OnProfilerDataCollected,
                       base::Unretained(this), sequence_number, profiler_data,
                       process_type));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (subscriber_) {
    subscriber_->OnProfilerDataCollected(sequence_number, profiler_data,
                                         process_type);
  }
}

void ProfilerControllerImpl::Register(ProfilerSubscriber* subscriber) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!subscriber_);
  subscriber_ = subscriber;
}

void ProfilerControllerImpl::Unregister(const ProfilerSubscriber* subscriber) {
  DCHECK_EQ(subscriber_, subscriber);
  subscriber_ = NULL;
}

void ProfilerControllerImpl::GetProfilerDataFromChildProcesses(
    int sequence_number,
    int current_profiling_phase) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  int pending_processes = 0;
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    // In some cases, there may be no child process of the given type (for
    // example, the GPU process may not exist and there may instead just be a
    // GPU thread in the browser process). If that's the case, then the process
    // handle will be base::kNullProcessHandle and we shouldn't ask it for data.
    if (iter.GetData().handle == base::kNullProcessHandle)
      continue;

    ++pending_processes;
    if (!iter.Send(new ChildProcessMsg_GetChildProfilerData(
            sequence_number, current_profiling_phase))) {
      --pending_processes;
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&ProfilerControllerImpl::OnPendingProcesses,
                     base::Unretained(this), sequence_number, pending_processes,
                     true));
}

// static
void ProfilerControllerImpl::NotifyChildProcessesOfProfilingPhaseCompletion(
    int profiling_phase) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    // In some cases, there may be no child process of the given type (for
    // example, the GPU process may not exist and there may instead just be a
    // GPU thread in the browser process). If that's the case, then the process
    // handle will be base::kNullProcessHandle and we shouldn't send it a
    // message.
    if (iter.GetData().handle == base::kNullProcessHandle)
      continue;

    iter.Send(new ChildProcessMsg_ProfilingPhaseCompleted(profiling_phase));
  }
}

void ProfilerControllerImpl::GetProfilerData(int sequence_number,
                                             int current_profiling_phase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Iterates through renderers in UI thread, and through other child processes
  // in IO thread, and send them GetChildProfilerData message. Renderers have to
  // be contacted from UI thread, and other processes - from IO thread.
  int pending_processes = 0;
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    ++pending_processes;
    if (!it.GetCurrentValue()->Send(new ChildProcessMsg_GetChildProfilerData(
            sequence_number, current_profiling_phase))) {
      --pending_processes;
    }
  }
  OnPendingProcesses(sequence_number, pending_processes, false);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ProfilerControllerImpl::GetProfilerDataFromChildProcesses,
                     base::Unretained(this), sequence_number,
                     current_profiling_phase));
}

void ProfilerControllerImpl::OnProfilingPhaseCompleted(int profiling_phase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Iterates through renderers in UI thread, and through other child processes
  // in IO thread, and send them OnProfilingPhase message. Renderers have to be
  // contacted from UI thread, and other processes - from IO thread.
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->Send(
        new ChildProcessMsg_ProfilingPhaseCompleted(profiling_phase));
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ProfilerControllerImpl::
                         NotifyChildProcessesOfProfilingPhaseCompletion,
                     profiling_phase));
}

}  // namespace content
