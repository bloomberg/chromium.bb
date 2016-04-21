// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/perf_output.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/task_runner_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"

namespace {

// Create a dbus::FileDescriptor from a base::File.
dbus::ScopedFileDescriptor CreateFileDescriptor(base::File pipe_write_end) {
  dbus::ScopedFileDescriptor file_descriptor(new dbus::FileDescriptor);
  file_descriptor->PutValue(pipe_write_end.TakePlatformFile());
  file_descriptor->CheckValidity();
  return file_descriptor;
}

}  // namespace

PerfOutputCall::PerfOutputCall(
    scoped_refptr<base::TaskRunner> blocking_task_runner,
    base::TimeDelta duration,
    const std::vector<std::string>& perf_args,
    const DoneCallback& callback)
    : blocking_task_runner_(blocking_task_runner),
      duration_(duration),
      perf_args_(perf_args),
      done_callback_(callback),
      weak_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());

  perf_data_pipe_reader_.reset(new chromeos::PipeReaderForString(
      blocking_task_runner_,
      base::Bind(&PerfOutputCall::OnIOComplete, weak_factory_.GetWeakPtr())));

  base::File pipe_write_end = perf_data_pipe_reader_->StartIO();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&CreateFileDescriptor, base::Passed(&pipe_write_end)),
      base::Bind(&PerfOutputCall::OnFileDescriptorCreated,
                 weak_factory_.GetWeakPtr()));
}

PerfOutputCall::~PerfOutputCall() {}

void PerfOutputCall::OnFileDescriptorCreated(
    dbus::ScopedFileDescriptor file_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(file_descriptor);

  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();

  client->GetPerfOutput(duration_, perf_args_, std::move(file_descriptor),
                        base::Bind(&PerfOutputCall::OnGetPerfOutputError,
                                   weak_factory_.GetWeakPtr()));
}

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
