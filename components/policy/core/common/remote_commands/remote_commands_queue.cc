// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_queue.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

RemoteCommandsQueue::RemoteCommandsQueue()
    : clock_(new base::DefaultTickClock()) {
}

RemoteCommandsQueue::~RemoteCommandsQueue() {
  while (!incoming_commands_.empty())
    incoming_commands_.pop();
  if (running_command_)
    running_command_->Terminate();
}

void RemoteCommandsQueue::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void RemoteCommandsQueue::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void RemoteCommandsQueue::AddJob(std::unique_ptr<RemoteCommandJob> job) {
  incoming_commands_.push(linked_ptr<RemoteCommandJob>(job.release()));

  if (!running_command_)
    ScheduleNextJob();
}

void RemoteCommandsQueue::SetClockForTesting(
    std::unique_ptr<base::TickClock> clock) {
  clock_ = std::move(clock);
}

base::TimeTicks RemoteCommandsQueue::GetNowTicks() {
  return clock_->NowTicks();
}

void RemoteCommandsQueue::OnCommandTimeout() {
  DCHECK(running_command_);

  // Calling Terminate() will also trigger CurrentJobFinished() below.
  running_command_->Terminate();
}

void RemoteCommandsQueue::CurrentJobFinished() {
  DCHECK(running_command_);

  execution_timeout_timer_.Stop();

  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnJobFinished(running_command_.get()));
  running_command_.reset();

  ScheduleNextJob();
}

void RemoteCommandsQueue::ScheduleNextJob() {
  DCHECK(!running_command_);
  if (incoming_commands_.empty())
    return;
  DCHECK(!execution_timeout_timer_.IsRunning());

  running_command_.reset(incoming_commands_.front().release());
  incoming_commands_.pop();

  execution_timeout_timer_.Start(FROM_HERE,
                                 running_command_->GetCommmandTimeout(), this,
                                 &RemoteCommandsQueue::OnCommandTimeout);

  if (running_command_->Run(clock_->NowTicks(),
                            base::Bind(&RemoteCommandsQueue::CurrentJobFinished,
                                       base::Unretained(this)))) {
    FOR_EACH_OBSERVER(Observer, observer_list_,
                      OnJobStarted(running_command_.get()));
  } else {
    CurrentJobFinished();
  }
}

}  // namespace policy
