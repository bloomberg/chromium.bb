// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service_impl.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_host_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace arc {

extern ArcBridgeService* g_arc_bridge_service;

namespace {
constexpr int64_t kReconnectDelayInSeconds = 5;
}  // namespace

ArcBridgeServiceImpl::ArcBridgeServiceImpl(
    std::unique_ptr<ArcBridgeBootstrap> bootstrap)
    : bootstrap_(std::move(bootstrap)),
      session_started_(false),
      weak_factory_(this) {
  DCHECK(!g_arc_bridge_service);
  g_arc_bridge_service = this;
  bootstrap_->set_delegate(this);
}

ArcBridgeServiceImpl::~ArcBridgeServiceImpl() {
  DCHECK(g_arc_bridge_service == this);
  g_arc_bridge_service = nullptr;
}

void ArcBridgeServiceImpl::HandleStartup() {
  DCHECK(CalledOnValidThread());
  if (session_started_)
    return;
  VLOG(1) << "Session started";
  session_started_ = true;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::Shutdown() {
  DCHECK(CalledOnValidThread());
  if (!session_started_)
    return;
  VLOG(1) << "Session ended";
  session_started_ = false;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::DisableReconnectDelayForTesting() {
  use_delay_before_reconnecting_ = false;
}

void ArcBridgeServiceImpl::PrerequisitesChanged() {
  DCHECK(CalledOnValidThread());
  VLOG(1) << "Prerequisites changed. "
          << "state=" << static_cast<uint32_t>(state())
          << ", session_started=" << session_started_;
  if (state() == State::STOPPED) {
    if (!session_started_)
      return;
    VLOG(0) << "Prerequisites met, starting ARC";
    SetStopReason(StopReason::SHUTDOWN);
    SetState(State::CONNECTING);
    bootstrap_->Start();
  } else {
    if (session_started_)
      return;
    VLOG(0) << "Prerequisites stopped being met, stopping ARC";
    StopInstance();
  }
}

void ArcBridgeServiceImpl::StopInstance() {
  DCHECK(CalledOnValidThread());
  if (state() == State::STOPPED || state() == State::STOPPING) {
    VLOG(1) << "StopInstance() called when ARC is not running";
    return;
  }

  VLOG(1) << "Stopping ARC";
  SetState(State::STOPPING);
  arc_bridge_host_.reset();
  bootstrap_->Stop();

  // We were explicitly asked to stop, so do not reconnect.
  reconnect_ = false;
}

void ArcBridgeServiceImpl::OnConnectionEstablished(
    mojom::ArcBridgeInstancePtr instance) {
  DCHECK(CalledOnValidThread());
  if (state() != State::CONNECTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }

  arc_bridge_host_.reset(new ArcBridgeHostImpl(std::move(instance)));

  // The container can be considered to have been successfully launched, so
  // restart if the connection goes down without being requested.
  reconnect_ = true;
  VLOG(0) << "ARC ready";
  SetState(State::READY);
}

void ArcBridgeServiceImpl::OnStopped(StopReason stop_reason) {
  DCHECK(CalledOnValidThread());
  arc_bridge_host_.reset();
  SetStopReason(stop_reason);
  SetState(State::STOPPED);
  VLOG(0) << "ARC stopped";

  if (reconnect_) {
    // There was a previous invocation and it crashed for some reason. Try
    // starting the container again.
    reconnect_ = false;
    VLOG(0) << "ARC reconnecting";
    if (use_delay_before_reconnecting_) {
      // Instead of immediately trying to restart the container, give it some
      // time to finish tearing down in case it is still in the process of
      // stopping.
      base::MessageLoop::current()->task_runner()->PostDelayedTask(
          FROM_HERE, base::Bind(&ArcBridgeServiceImpl::PrerequisitesChanged,
                                weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(kReconnectDelayInSeconds));
    } else {
      // Restart immediately.
      PrerequisitesChanged();
    }
  }
}

}  // namespace arc
