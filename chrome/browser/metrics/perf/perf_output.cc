// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/perf_output.h"

#include "base/bind.h"
#include "base/task_scheduler/post_task.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"

PerfOutputCall::PerfOutputCall(base::TimeDelta duration,
                               const std::vector<std::string>& perf_args,
                               const DoneCallback& callback)
    : duration_(duration),
      perf_args_(perf_args),
      done_callback_(callback),
      weak_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());

  perf_data_pipe_reader_.reset(new chromeos::PipeReaderForString(
      base::CreateTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
      base::Bind(&PerfOutputCall::OnIOComplete, weak_factory_.GetWeakPtr())));

  base::ScopedFD pipe_write_end = perf_data_pipe_reader_->StartIO();
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->GetPerfOutput(duration_, perf_args_, pipe_write_end.get(),
                        base::Bind(&PerfOutputCall::OnGetPerfOutputError,
                                   weak_factory_.GetWeakPtr()));
}

PerfOutputCall::~PerfOutputCall() {}

void PerfOutputCall::OnIOComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string stdout_data;
  perf_data_pipe_reader_->GetData(&stdout_data);

  done_callback_.Run(stdout_data);
  // The callback may delete us, so it's hammertime: Can't touch |this|.
}

void PerfOutputCall::OnGetPerfOutputError(const std::string& error_name,
                                          const std::string& error_message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Signal pipe reader to shut down. This will cause
  // OnIOComplete to be called, probably with an empty string.
  perf_data_pipe_reader_->OnDataReady(-1);
}
