// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service_impl.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/sequenced_task_runner.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_session.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace arc {

namespace {

constexpr base::TimeDelta kDefaultRestartDelay =
    base::TimeDelta::FromSeconds(5);

}  // namespace

ArcBridgeServiceImpl::ArcBridgeServiceImpl(
    const scoped_refptr<base::TaskRunner>& blocking_task_runner)
    : session_started_(false),
      restart_delay_(kDefaultRestartDelay),
      factory_(base::Bind(ArcSession::Create, this, blocking_task_runner)),
      weak_factory_(this) {}

ArcBridgeServiceImpl::~ArcBridgeServiceImpl() {
  if (arc_session_)
    arc_session_->RemoveObserver(this);
}

void ArcBridgeServiceImpl::RequestStart() {
  DCHECK(CalledOnValidThread());
  if (session_started_)
    return;
  VLOG(1) << "Session started";
  session_started_ = true;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::RequestStop() {
  DCHECK(CalledOnValidThread());
  if (!session_started_)
    return;
  VLOG(1) << "Session ended";
  session_started_ = false;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::OnShutdown() {
  DCHECK(CalledOnValidThread());

  VLOG(1) << "OnShutdown";
  session_started_ = false;
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
  factory_ = factory;
}

void ArcBridgeServiceImpl::SetRestartDelayForTesting(
    const base::TimeDelta& restart_delay) {
  DCHECK_EQ(state(), State::STOPPED);
  DCHECK(!arc_session_);
  DCHECK(!restart_timer_.IsRunning());
  restart_delay_ = restart_delay;
}

void ArcBridgeServiceImpl::PrerequisitesChanged() {
  DCHECK(CalledOnValidThread());
  VLOG(1) << "Prerequisites changed. "
          << "state=" << static_cast<uint32_t>(state())
          << ", session_started=" << session_started_;
  if (state() == State::STOPPED) {
    if (!session_started_) {
      // We were explicitly asked to stop, so do not restart.
      restart_timer_.Stop();
      return;
    }
    DCHECK(!restart_timer_.IsRunning());
    VLOG(0) << "Prerequisites met, starting ARC";
    StartArcSession();
  } else {
    DCHECK(!restart_timer_.IsRunning());
    if (session_started_)
      return;
    VLOG(0) << "Prerequisites stopped being met, stopping ARC";
    StopInstance();
  }
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
  SetState(State::CONNECTING);
  arc_session_->Start();
}

void ArcBridgeServiceImpl::StopInstance() {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(state(), State::STOPPED);
  DCHECK(!restart_timer_.IsRunning());

  if (state() == State::STOPPING) {
    VLOG(1) << "StopInstance() called when ARC is not running";
    return;
  }

  VLOG(1) << "Stopping ARC";
  DCHECK(arc_session_.get());
  SetState(State::STOPPING);

  // Note: this can call OnStopped() internally as a callback.
  arc_session_->Stop();
}

void ArcBridgeServiceImpl::OnSessionReady() {
  DCHECK(CalledOnValidThread());
  if (state() != State::CONNECTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }

  VLOG(0) << "ARC ready";
  SetState(State::READY);
}

void ArcBridgeServiceImpl::OnSessionStopped(StopReason stop_reason) {
  DCHECK(CalledOnValidThread());
  DCHECK(!restart_timer_.IsRunning());

  VLOG(0) << "ARC stopped: " << stop_reason;
  arc_session_->RemoveObserver(this);
  arc_session_.reset();

  // If READY, ARC instance unexpectedly crashed so we need to restart it
  // automatically.
  // If STOPPING, at least once RequestStop() is called. If |session_started_|
  // is true, RequestStart() is following so schedule to restart ARC session.
  // Otherwise, do nothing.
  // If CONNECTING, ARC instance has not been booted properly, so do not
  // restart it automatically.
  if (state() == State::READY ||
      (state() == State::STOPPING && session_started_)) {
    // This check is for READY case. In READY case |session_started_| should be
    // always true, because if once RequestStop() is called, the state() will
    // be set to STOPPING.
    DCHECK(session_started_);

    // There was a previous invocation and it crashed for some reason. Try
    // starting ARC instance later again.
    // Note that even |restart_delay_| is 0 (for testing), it needs to
    // PostTask, because observer callback may call RequestStart()/Stop(),
    // which can change restarting.
    VLOG(0) << "ARC restarting";
    restart_timer_.Start(FROM_HERE, restart_delay_,
                         base::Bind(&ArcBridgeServiceImpl::StartArcSession,
                                    weak_factory_.GetWeakPtr()));
  }

  // TODO(hidehiko): Consider to let observers know whether there is scheduled
  // restarting event, or not.
  SetStopReason(stop_reason);
  SetState(State::STOPPED);
}

}  // namespace arc
