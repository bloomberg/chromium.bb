// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_session_runner.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"

namespace arc {

namespace {

constexpr base::TimeDelta kDefaultRestartDelay =
    base::TimeDelta::FromSeconds(5);

}  // namespace

ArcSessionRunner::ArcSessionRunner(const ArcSessionFactory& factory)
    : restart_delay_(kDefaultRestartDelay),
      factory_(factory),
      weak_ptr_factory_(this) {}

ArcSessionRunner::~ArcSessionRunner() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (arc_session_)
    arc_session_->RemoveObserver(this);
}

void ArcSessionRunner::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void ArcSessionRunner::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void ArcSessionRunner::RequestStart() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Consecutive RequestStart() call. Do nothing.
  if (run_requested_)
    return;

  VLOG(1) << "Session started";
  run_requested_ = true;
  // Here |run_requested_| transitions from false to true. So, |restart_timer_|
  // must be stopped (either not even started, or has been cancelled in
  // previous RequestStop() call).
  DCHECK(!restart_timer_.IsRunning());

  if (arc_session_) {
    // In this case, RequestStop() was called, and before |arc_session_| had
    // finished stopping, RequestStart() was called. Do nothing in that case,
    // since when |arc_session_| does actually stop, OnSessionStopped() will
    // be called, where it should automatically restart.
    DCHECK_EQ(state_, State::STOPPING);
  } else {
    DCHECK_EQ(state_, State::STOPPED);
    StartArcSession();
  }
}

void ArcSessionRunner::RequestStop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Consecutive RequestStop() call. Do nothing.
  if (!run_requested_)
    return;

  VLOG(1) << "Session ended";
  run_requested_ = false;

  if (arc_session_) {
    // The |state_| could be either STARTING, RUNNING or STOPPING.
    DCHECK_NE(state_, State::STOPPED);

    if (state_ == State::STOPPING) {
      // STOPPING is found in the senario of "RequestStart() -> RequestStop()
      // -> RequestStart() -> RequestStop()" case.
      // In the first RequestStop() call, |state_| is set to STOPPING,
      // and in the second RequestStop() finds it (so this is the second call).
      // In that case, ArcSession::Stop() is already called, so do nothing.
      return;
    }
    state_ = State::STOPPING;
    arc_session_->Stop();
  } else {
    DCHECK_EQ(state_, State::STOPPED);
    // In case restarting is in progress, cancel it.
    restart_timer_.Stop();
  }
}

void ArcSessionRunner::OnShutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "OnShutdown";
  run_requested_ = false;
  restart_timer_.Stop();
  if (arc_session_) {
    DCHECK_NE(state_, State::STOPPED);
    state_ = State::STOPPING;
    arc_session_->OnShutdown();
  }
  // ArcSession::OnShutdown() invokes OnSessionStopped() synchronously.
  // In the observer method, |arc_session_| should be destroyed.
  DCHECK(!arc_session_);
}

bool ArcSessionRunner::IsRunning() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return state_ == State::RUNNING;
}

bool ArcSessionRunner::IsStopped() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return state_ == State::STOPPED;
}

void ArcSessionRunner::SetRestartDelayForTesting(
    const base::TimeDelta& restart_delay) {
  DCHECK_EQ(state_, State::STOPPED);
  DCHECK(!arc_session_);
  DCHECK(!restart_timer_.IsRunning());
  restart_delay_ = restart_delay;
}

void ArcSessionRunner::StartArcSession() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::STOPPED);
  DCHECK(!arc_session_);
  DCHECK(!restart_timer_.IsRunning());

  VLOG(1) << "Starting ARC instance";
  arc_session_ = factory_.Run();
  arc_session_->AddObserver(this);
  state_ = State::STARTING;
  arc_session_->Start();
}

void ArcSessionRunner::OnSessionReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::STARTING);
  DCHECK(arc_session_);
  DCHECK(!restart_timer_.IsRunning());

  VLOG(0) << "ARC ready";
  state_ = State::RUNNING;
}

void ArcSessionRunner::OnSessionStopped(ArcStopReason stop_reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(state_, State::STOPPED);
  DCHECK(arc_session_);
  DCHECK(!restart_timer_.IsRunning());

  VLOG(0) << "ARC stopped: " << stop_reason;
  arc_session_->RemoveObserver(this);
  arc_session_.reset();

  // If RUNNING, ARC instance unexpectedly crashed so we need to restart it
  // automatically.
  // If STOPPING, at least once RequestStop() is called. If |session_started_|
  // is true, RequestStart() is following so schedule to restart ARC session.
  // Otherwise, do nothing.
  // If STARTING, ARC instance has not been booted properly, so do not
  // restart it automatically.
  const bool restarting = (state_ == State::RUNNING ||
                           (state_ == State::STOPPING && run_requested_));
  if (restarting) {
    // This check is for RUNNING case. In RUNNING case |run_requested_| should
    // be always true, because if once RequestStop() is called, the state_
    // will be set to STOPPING.
    DCHECK(run_requested_);

    // There was a previous invocation and it crashed for some reason. Try
    // starting ARC instance later again.
    // Note that even |restart_delay_| is 0 (for testing), it needs to
    // PostTask, because observer callback may call RequestStart()/Stop().
    VLOG(0) << "ARC restarting";
    restart_timer_.Start(FROM_HERE, restart_delay_,
                         base::Bind(&ArcSessionRunner::StartArcSession,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  state_ = State::STOPPED;
  for (auto& observer : observer_list_)
    observer.OnSessionStopped(stop_reason, restarting);
}

}  // namespace arc
