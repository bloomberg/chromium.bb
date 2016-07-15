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
      binding_(this),
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
          << ", available=" << available()
          << ", session_started=" << session_started_;
  if (state() == State::STOPPED) {
    if (!available() || !session_started_)
      return;
    VLOG(0) << "Prerequisites met, starting ARC";
    SetStopReason(StopReason::SHUTDOWN);
    SetState(State::CONNECTING);
    bootstrap_->Start();
  } else {
    if (available() && session_started_)
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
  instance_ptr_.reset();
  if (binding_.is_bound())
    binding_.Close();
  bootstrap_->Stop();

  // We were explicitly asked to stop, so do not reconnect.
  reconnect_ = false;
}

void ArcBridgeServiceImpl::SetDetectedAvailability(bool arc_available) {
  DCHECK(CalledOnValidThread());
  if (available() == arc_available)
    return;
  VLOG(1) << "ARC available: " << arc_available;
  SetAvailable(arc_available);
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::OnConnectionEstablished(
    mojom::ArcBridgeInstancePtr instance) {
  DCHECK(CalledOnValidThread());
  if (state() != State::CONNECTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }

  instance_ptr_ = std::move(instance);
  instance_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeServiceImpl::OnChannelClosed, weak_factory_.GetWeakPtr()));

  instance_ptr_->Init(binding_.CreateInterfacePtrAndBind());

  // The container can be considered to have been successfully launched, so
  // restart if the connection goes down without being requested.
  reconnect_ = true;
  VLOG(0) << "ARC ready";
  SetState(State::READY);
}

void ArcBridgeServiceImpl::OnStopped(StopReason stop_reason) {
  DCHECK(CalledOnValidThread());
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

void ArcBridgeServiceImpl::OnChannelClosed() {
  DCHECK(CalledOnValidThread());
  if (state() == State::STOPPED || state() == State::STOPPING) {
    // This will happen when the instance is shut down. Ignore that case.
    return;
  }
  VLOG(1) << "Mojo connection lost";
  instance_ptr_.reset();
  if (binding_.is_bound())
    binding_.Close();

  // Call all the error handlers of all the channels to both close the channel
  // and notify any observers that the channel is closed.
  app_.CloseChannel();
  audio_.CloseChannel();
  auth_.CloseChannel();
  bluetooth_.CloseChannel();
  clipboard_.CloseChannel();
  crash_collector_.CloseChannel();
  file_system_.CloseChannel();
  ime_.CloseChannel();
  intent_helper_.CloseChannel();
  metrics_.CloseChannel();
  net_.CloseChannel();
  notifications_.CloseChannel();
  obb_mounter_.CloseChannel();
  policy_.CloseChannel();
  power_.CloseChannel();
  process_.CloseChannel();
  storage_manager_.CloseChannel();
  video_.CloseChannel();
  window_manager_.CloseChannel();
}

void ArcBridgeServiceImpl::OnAppInstanceReady(mojom::AppInstancePtr app_ptr) {
  DCHECK(CalledOnValidThread());
  app_.OnInstanceReady(std::move(app_ptr));
}

void ArcBridgeServiceImpl::OnAudioInstanceReady(
    mojom::AudioInstancePtr audio_ptr) {
  DCHECK(CalledOnValidThread());
  audio_.OnInstanceReady(std::move(audio_ptr));
}

void ArcBridgeServiceImpl::OnAuthInstanceReady(
    mojom::AuthInstancePtr auth_ptr) {
  DCHECK(CalledOnValidThread());
  auth_.OnInstanceReady(std::move(auth_ptr));
}

void ArcBridgeServiceImpl::OnBluetoothInstanceReady(
    mojom::BluetoothInstancePtr bluetooth_ptr) {
  DCHECK(CalledOnValidThread());
  bluetooth_.OnInstanceReady(std::move(bluetooth_ptr));
}

void ArcBridgeServiceImpl::OnClipboardInstanceReady(
    mojom::ClipboardInstancePtr clipboard_ptr) {
  DCHECK(CalledOnValidThread());
  clipboard_.OnInstanceReady(std::move(clipboard_ptr));
}

void ArcBridgeServiceImpl::OnCrashCollectorInstanceReady(
    mojom::CrashCollectorInstancePtr crash_collector_ptr) {
  DCHECK(CalledOnValidThread());
  crash_collector_.OnInstanceReady(std::move(crash_collector_ptr));
}

void ArcBridgeServiceImpl::OnFileSystemInstanceReady(
    mojom::FileSystemInstancePtr file_system_ptr) {
  DCHECK(CalledOnValidThread());
  file_system_.OnInstanceReady(std::move(file_system_ptr));
}

void ArcBridgeServiceImpl::OnImeInstanceReady(mojom::ImeInstancePtr ime_ptr) {
  DCHECK(CalledOnValidThread());
  ime_.OnInstanceReady(std::move(ime_ptr));
}

void ArcBridgeServiceImpl::OnIntentHelperInstanceReady(
    mojom::IntentHelperInstancePtr intent_helper_ptr) {
  DCHECK(CalledOnValidThread());
  intent_helper_.OnInstanceReady(std::move(intent_helper_ptr));
}

void ArcBridgeServiceImpl::OnMetricsInstanceReady(
    mojom::MetricsInstancePtr metrics_ptr) {
  DCHECK(CalledOnValidThread());
  metrics_.OnInstanceReady(std::move(metrics_ptr));
}

void ArcBridgeServiceImpl::OnNetInstanceReady(mojom::NetInstancePtr net_ptr) {
  DCHECK(CalledOnValidThread());
  net_.OnInstanceReady(std::move(net_ptr));
}

void ArcBridgeServiceImpl::OnNotificationsInstanceReady(
    mojom::NotificationsInstancePtr notifications_ptr) {
  DCHECK(CalledOnValidThread());
  notifications_.OnInstanceReady(std::move(notifications_ptr));
}

void ArcBridgeServiceImpl::OnObbMounterInstanceReady(
    mojom::ObbMounterInstancePtr obb_mounter_ptr) {
  DCHECK(CalledOnValidThread());
  obb_mounter_.OnInstanceReady(std::move(obb_mounter_ptr));
}

void ArcBridgeServiceImpl::OnPolicyInstanceReady(
    mojom::PolicyInstancePtr policy_ptr) {
  DCHECK(CalledOnValidThread());
  policy_.OnInstanceReady(std::move(policy_ptr));
}

void ArcBridgeServiceImpl::OnPowerInstanceReady(
    mojom::PowerInstancePtr power_ptr) {
  DCHECK(CalledOnValidThread());
  power_.OnInstanceReady(std::move(power_ptr));
}

void ArcBridgeServiceImpl::OnProcessInstanceReady(
    mojom::ProcessInstancePtr process_ptr) {
  DCHECK(CalledOnValidThread());
  process_.OnInstanceReady(std::move(process_ptr));
}

void ArcBridgeServiceImpl::OnStorageManagerInstanceReady(
    mojom::StorageManagerInstancePtr storage_manager_ptr) {
  DCHECK(CalledOnValidThread());
  storage_manager_.OnInstanceReady(std::move(storage_manager_ptr));
}

void ArcBridgeServiceImpl::OnVideoInstanceReady(
    mojom::VideoInstancePtr video_ptr) {
  DCHECK(CalledOnValidThread());
  video_.OnInstanceReady(std::move(video_ptr));
}

void ArcBridgeServiceImpl::OnWindowManagerInstanceReady(
    mojom::WindowManagerInstancePtr window_manager_ptr) {
  DCHECK(CalledOnValidThread());
  window_manager_.OnInstanceReady(std::move(window_manager_ptr));
}

}  // namespace arc
