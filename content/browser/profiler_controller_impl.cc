// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/profiler_controller_impl.h"

#include "base/bind.h"
#include "base/values.h"
#include "content/browser/browser_child_process_host.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/profiler_subscriber.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"

using content::BrowserThread;

namespace content {

content::ProfilerController* content::ProfilerController::GetInstance() {
  return ProfilerControllerImpl::GetInstance();
}

ProfilerControllerImpl* ProfilerControllerImpl::GetInstance() {
  return Singleton<ProfilerControllerImpl>::get();
}

ProfilerControllerImpl::ProfilerControllerImpl() : subscriber_(NULL) {
}

ProfilerControllerImpl::~ProfilerControllerImpl() {
}

void ProfilerControllerImpl::OnPendingProcesses(int sequence_number,
                                                int pending_processes,
                                                bool end) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (subscriber_)
    subscriber_->OnPendingProcesses(sequence_number, pending_processes, end);
}

void ProfilerControllerImpl::OnProfilerDataCollected(
    int sequence_number,
    base::DictionaryValue* profiler_data) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ProfilerControllerImpl::OnProfilerDataCollected,
                   base::Unretained(this),
                   sequence_number,
                   profiler_data));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (subscriber_)
    subscriber_->OnProfilerDataCollected(sequence_number, profiler_data);
}

void ProfilerControllerImpl::Register(ProfilerSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!subscriber_);
  subscriber_ = subscriber;
}

void ProfilerControllerImpl::Unregister(ProfilerSubscriber* subscriber) {
  if (subscriber == subscriber_)
    subscriber_ = NULL;
}

void ProfilerControllerImpl::GetProfilerDataFromChildProcesses(
    int sequence_number) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  int pending_processes = 0;
  for (BrowserChildProcessHost::Iterator child_process_host;
       !child_process_host.Done(); ++child_process_host) {
    const std::string process_type =
      content::GetProcessTypeNameInEnglish(child_process_host->type());
    ++pending_processes;
    if (!child_process_host->Send(new ChildProcessMsg_GetChildProfilerData(
        sequence_number, process_type))) {
      --pending_processes;
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &ProfilerControllerImpl::OnPendingProcesses,
          base::Unretained(this),
          sequence_number,
          pending_processes,
          true));
}

void ProfilerControllerImpl::GetProfilerData(int sequence_number) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int pending_processes = 0;
  const std::string render_process_type =
      content::GetProcessTypeNameInEnglish(content::PROCESS_TYPE_RENDERER);

  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    ++pending_processes;
    if (!it.GetCurrentValue()->Send(new ChildProcessMsg_GetChildProfilerData(
        sequence_number, render_process_type))) {
      --pending_processes;
    }
  }
  OnPendingProcesses(sequence_number, pending_processes, false);

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ProfilerControllerImpl::GetProfilerDataFromChildProcesses,
                 base::Unretained(this),
                 sequence_number));
}

void ProfilerControllerImpl::SetProfilerStatusInChildProcesses(bool enable) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (BrowserChildProcessHost::Iterator child_process_host;
       !child_process_host.Done(); ++child_process_host) {
    child_process_host->Send(new ChildProcessMsg_SetProfilerStatus(enable));
  }
}

void ProfilerControllerImpl::SetProfilerStatus(bool enable) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ProfilerControllerImpl::SetProfilerStatusInChildProcesses,
                 base::Unretained(this),
                 enable));

  for (content::RenderProcessHost::iterator it(
          content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->Send(new ChildProcessMsg_SetProfilerStatus(enable));
  }
}
}  // namespace content
