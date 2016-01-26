// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service_impl.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/sequenced_task_runner.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"

namespace arc {

ArcBridgeServiceImpl::ArcBridgeServiceImpl(
    scoped_ptr<ArcBridgeBootstrap> bootstrap)
    : bootstrap_(std::move(bootstrap)),
      binding_(this),
      session_started_(false),
      weak_factory_(this) {
  bootstrap_->set_delegate(this);
}

ArcBridgeServiceImpl::~ArcBridgeServiceImpl() {
}

void ArcBridgeServiceImpl::DetectAvailability() {
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->CheckArcAvailability(base::Bind(
      &ArcBridgeServiceImpl::OnArcAvailable, weak_factory_.GetWeakPtr()));
}

void ArcBridgeServiceImpl::HandleStartup() {
  DCHECK(CalledOnValidThread());
  session_started_ = true;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::Shutdown() {
  DCHECK(CalledOnValidThread());
  session_started_ = false;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::PrerequisitesChanged() {
  DCHECK(CalledOnValidThread());
  if (state() == State::STOPPED) {
    if (!available() || !session_started_)
      return;
    SetState(State::CONNECTING);
    bootstrap_->Start();
  } else {
    if (available() && session_started_)
      return;
    StopInstance();
  }
}

void ArcBridgeServiceImpl::StopInstance() {
  DCHECK(CalledOnValidThread());
  if (state() == State::STOPPED || state() == State::STOPPING) {
    VLOG(1) << "StopInstance() called when ARC is not running";
    return;
  }

  SetState(State::STOPPING);
  instance_ptr_.reset();
  if (binding_.is_bound())
    binding_.Close();
  bootstrap_->Stop();
}

void ArcBridgeServiceImpl::OnArcAvailable(bool arc_available) {
  DCHECK(CalledOnValidThread());
  if (available() == arc_available)
    return;
  SetAvailable(arc_available);
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::OnConnectionEstablished(
    ArcBridgeInstancePtr instance) {
  DCHECK(CalledOnValidThread());
  if (state() != State::CONNECTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }

  instance_ptr_ = std::move(instance);
  instance_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeServiceImpl::OnChannelClosed, weak_factory_.GetWeakPtr()));

  instance_ptr_->Init(binding_.CreateInterfacePtrAndBind());

  SetState(State::READY);
}

void ArcBridgeServiceImpl::OnStopped() {
  DCHECK(CalledOnValidThread());
  SetState(State::STOPPED);
  if (reconnect_) {
    // There was a previous invocation and it crashed for some reason. Try
    // starting the container again.
    reconnect_ = false;
    PrerequisitesChanged();
  }
}

void ArcBridgeServiceImpl::OnChannelClosed() {
  DCHECK(CalledOnValidThread());
  if (state() == State::STOPPED || state() == State::STOPPING) {
    // This will happen when the instance is shut down. Ignore that case.
    return;
  }
  VLOG(1) << "Mojo connection lost";
  CloseAllChannels();
  reconnect_ = true;
  StopInstance();
}

}  // namespace arc
