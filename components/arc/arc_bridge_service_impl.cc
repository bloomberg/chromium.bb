// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service_impl.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "components/arc/arc_session.h"

namespace arc {

namespace {

constexpr base::TimeDelta kDefaultRestartDelay =
    base::TimeDelta::FromSeconds(5);

}  // namespace

ArcBridgeServiceImpl::ArcBridgeServiceImpl(
    const scoped_refptr<base::TaskRunner>& blocking_task_runner)
    : restart_delay_(kDefaultRestartDelay),
      factory_(base::Bind(ArcSession::Create, this, blocking_task_runner)),
      weak_ptr_factory_(this) {}

ArcBridgeServiceImpl::~ArcBridgeServiceImpl() {
  DCHECK(CalledOnValidThread());
  if (arc_session_)
    arc_session_->RemoveObserver(this);
}

void ArcBridgeServiceImpl::RequestStart() {
  DCHECK(CalledOnValidThread());

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
    DCHECK_EQ(state(), State::STOPPING);
  } else {
    DCHECK_EQ(state(), State::STOPPED);
    StartArcSession();
  }
}

void ArcBridgeServiceImpl::RequestStop() {
  DCHECK(CalledOnValidThread());

  // Consecutive RequestStop() call. Do nothing.
  if (!run_requested_)
    return;

  VLOG(1) << "Session ended";
  run_requested_ = false;

  if (arc_session_) {
    // The |state_| could be either STARTING, RUNNING or STOPPING.
    DCHECK_NE(state(), State::STOPPED);

    if (state() == State::STOPPING) {
      // STOPPING is found in the senario of "RequestStart() -> RequestStop()
      // -> RequestStart() -> RequestStop()" case.
      // In the first RequestStop() call, |state_| is set to STOPPING,
      // and in the second RequestStop() finds it (so this is the second call).
      // In that case, ArcSession::Stop() is already called, so do nothing.
      return;
    }
    SetState(State::STOPPING);
    arc_session_->Stop();
  } else {
    DCHECK_EQ(state(), State::STOPPED);
    // In case restarting is in progress, cancel it.
    restart_timer_.Stop();
  }
}

void ArcBridgeServiceImpl::OnShutdown() {
  DCHECK(CalledOnValidThread());

  VLOG(1) << "OnShutdown";
  run_requested_ = false;
  restart_timer_.Stop();
  if (arc_session_) {
    if (state() != State::STOPPING)
      SetState(State::STOPPING);
    arc_session_->OnShutdown();
  }
  // ArcSession::OnShutdown() invokes OnSessionStopped() synchronously.
  // In the observer method, |arc_session_| should be destroyed.
  DCHECK(!arc_session_);
}

void ArcBridgeServiceImpl::SetArcSessionFactoryForTesting(
    const ArcSessionFactory& factory) {
  DCHECK(!factory.is_null());
  DCHECK_EQ(state(), State::STOPPED);
  DCHECK(!arc_session_);
  DCHECK(!restart_timer_.IsRunning());
  factory_ = factory;
}

void ArcBridgeServiceImpl::SetRestartDelayForTesting(
    const base::TimeDelta& restart_delay) {
  DCHECK_EQ(state(), State::STOPPED);
  DCHECK(!arc_session_);
  DCHECK(!restart_timer_.IsRunning());
  restart_delay_ = restart_delay;
}

void ArcBridgeServiceImpl::StartArcSession() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state(), State::STOPPED);
  DCHECK(!arc_session_);
  DCHECK(!restart_timer_.IsRunning());

  VLOG(1) << "Starting ARC instance";
  SetStopReason(StopReason::SHUTDOWN);
  arc_session_ = factory_.Run();
  arc_session_->AddObserver(this);
  SetState(State::STARTING);
  arc_session_->Start();
}

void ArcBridgeServiceImpl::OnSessionReady() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state(), State::STARTING);
  DCHECK(arc_session_);
  DCHECK(!restart_timer_.IsRunning());

  VLOG(0) << "ARC ready";
  SetState(State::RUNNING);
}

void ArcBridgeServiceImpl::OnSessionStopped(StopReason stop_reason) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(state(), State::STOPPED);
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
  if (state() == State::RUNNING ||
      (state() == State::STOPPING && run_requested_)) {
    // This check is for READY case. In RUNNING case |run_requested_| should
    // be always true, because if once RequestStop() is called, the state()
    // will be set to STOPPING.
    DCHECK(run_requested_);

    // There was a previous invocation and it crashed for some reason. Try
    // starting ARC instance later again.
    // Note that even |restart_delay_| is 0 (for testing), it needs to
    // PostTask, because observer callback may call RequestStart()/Stop(),
    // which can change restarting.
    VLOG(0) << "ARC restarting";
    restart_timer_.Start(FROM_HERE, restart_delay_,
                         base::Bind(&ArcBridgeServiceImpl::StartArcSession,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  // TODO(hidehiko): Consider to let observers know whether there is scheduled
  // restarting event, or not.
  SetStopReason(stop_reason);
  SetState(State::STOPPED);
}

}  // namespace arc
