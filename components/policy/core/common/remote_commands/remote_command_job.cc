// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_command_job.h"

#include "base/bind.h"
#include "base/logging.h"

namespace policy {

namespace {

const int kDefaultCommandTimeoutInMinutes = 3;

}  // namespace

namespace em = enterprise_management;

RemoteCommandJob::~RemoteCommandJob() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (status_ == RUNNING)
    Terminate();
}

bool RemoteCommandJob::Init(const em::RemoteCommand& command) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(NOT_INITIALIZED, status_);

  status_ = INVALID;

  if (!command.has_type() || !command.has_unique_id())
    return false;
  DCHECK_EQ(command.type(), GetType());

  unique_id_ = command.unique_id();

  if (command.has_timestamp()) {
    issued_time_ = base::Time::UnixEpoch() +
                   base::TimeDelta::FromMilliseconds(command.timestamp());
  } else {
    LOG(WARNING) << "No issued_time provided be server for command "
                 << unique_id_ << ".";
  }

  if (!ParseCommandPayload(command.payload()))
    return false;

  status_ = NOT_STARTED;
  return true;
}

bool RemoteCommandJob::Run(base::Time now,
                           const FinishedCallback& finished_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (status_ == INVALID)
    return false;

  DCHECK_EQ(NOT_STARTED, status_);

  if (IsExpired(now)) {
    status_ = EXPIRED;
    return false;
  }

  execution_started_time_ = now;
  status_ = RUNNING;
  finished_callback_ = finished_callback;

  RunImpl(base::Bind(&RemoteCommandJob::OnCommandExecutionSucceeded,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&RemoteCommandJob::OnCommandExecutionFailed,
                     weak_factory_.GetWeakPtr()));

  // The command is expected to run asynchronously.
  DCHECK_EQ(RUNNING, status_);

  return true;
}

void RemoteCommandJob::Terminate() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (IsExecutionFinished())
    return;

  DCHECK_EQ(RUNNING, status_);

  status_ = TERMINATED;
  weak_factory_.InvalidateWeakPtrs();

  TerminateImpl();

  if (!finished_callback_.is_null())
    finished_callback_.Run();
}

base::TimeDelta RemoteCommandJob::GetCommmandTimeout() const {
  return base::TimeDelta::FromMinutes(kDefaultCommandTimeoutInMinutes);
}

bool RemoteCommandJob::IsExecutionFinished() const {
  return status_ == SUCCEEDED || status_ == FAILED || status_ == TERMINATED;
}

scoped_ptr<std::string> RemoteCommandJob::GetResultPayload() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(SUCCEEDED, status_);

  if (!result_payload_)
    return nullptr;

  return result_payload_->Serialize().Pass();
}

RemoteCommandJob::RemoteCommandJob()
    : status_(NOT_INITIALIZED), weak_factory_(this) {
}

bool RemoteCommandJob::ParseCommandPayload(const std::string& command_payload) {
  return true;
}

bool RemoteCommandJob::IsExpired(base::Time now) {
  return false;
}

void RemoteCommandJob::TerminateImpl() {
}

void RemoteCommandJob::OnCommandExecutionSucceeded(
    scoped_ptr<RemoteCommandJob::ResultPayload> result_payload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(RUNNING, status_);
  status_ = SUCCEEDED;

  result_payload_ = result_payload.Pass();

  if (!finished_callback_.is_null())
    finished_callback_.Run();
}

void RemoteCommandJob::OnCommandExecutionFailed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(RUNNING, status_);
  status_ = FAILED;

  if (!finished_callback_.is_null())
    finished_callback_.Run();
}

}  // namespace policy
