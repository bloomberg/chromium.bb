// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service_impl.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ArcServiceManager.
ArcBridgeService* g_arc_bridge_service = nullptr;

}  // namespace

ArcBridgeService::ArcBridgeService()
    : available_(false), state_(State::STOPPED), weak_factory_(this) {
  DCHECK(!g_arc_bridge_service);
  g_arc_bridge_service = this;
}

ArcBridgeService::~ArcBridgeService() {
  DCHECK(CalledOnValidThread());
  DCHECK(state() == State::STOPPING || state() == State::STOPPED);
  DCHECK(g_arc_bridge_service == this);
  g_arc_bridge_service = nullptr;
}

// static
ArcBridgeService* ArcBridgeService::Get() {
  DCHECK(g_arc_bridge_service);
  DCHECK(g_arc_bridge_service->CalledOnValidThread());
  return g_arc_bridge_service;
}

// static
bool ArcBridgeService::GetEnabled(const base::CommandLine* command_line) {
  return command_line->HasSwitch(chromeos::switches::kEnableArc);
}

void ArcBridgeService::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void ArcBridgeService::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void ArcBridgeService::OnAppInstanceReady(AppInstancePtr app_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_app_ptr_ = std::move(app_ptr);
  temporary_app_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnAppVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnAppVersionReady(int32_t version) {
  DCHECK(CalledOnValidThread());
  app_ptr_ = std::move(temporary_app_ptr_);
  FOR_EACH_OBSERVER(Observer, observer_list(), OnAppInstanceReady());
}

void ArcBridgeService::OnInputInstanceReady(InputInstancePtr input_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_input_ptr_ = std::move(input_ptr);
  temporary_input_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnInputVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnInputVersionReady(int32_t version) {
  DCHECK(CalledOnValidThread());
  input_ptr_ = std::move(temporary_input_ptr_);
  FOR_EACH_OBSERVER(Observer, observer_list(), OnInputInstanceReady());
}

void ArcBridgeService::OnNotificationsInstanceReady(
    NotificationsInstancePtr notifications_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_notifications_ptr_ = std::move(notifications_ptr);
  temporary_notifications_ptr_.QueryVersion(
      base::Bind(&ArcBridgeService::OnNotificationsVersionReady,
                 weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnNotificationsVersionReady(int32_t version) {
  DCHECK(CalledOnValidThread());
  notifications_ptr_ = std::move(temporary_notifications_ptr_);
  FOR_EACH_OBSERVER(Observer, observer_list(), OnNotificationsInstanceReady());
}

void ArcBridgeService::OnPowerInstanceReady(PowerInstancePtr power_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_power_ptr_ = std::move(power_ptr);
  temporary_power_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnPowerVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnPowerVersionReady(int32_t version) {
  DCHECK(CalledOnValidThread());
  power_ptr_ = std::move(temporary_power_ptr_);
  FOR_EACH_OBSERVER(Observer, observer_list(), OnPowerInstanceReady());
}

void ArcBridgeService::OnProcessInstanceReady(ProcessInstancePtr process_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_process_ptr_ = std::move(process_ptr);
  temporary_process_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnProcessVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnProcessVersionReady(int32_t version) {
  DCHECK(CalledOnValidThread());
  process_ptr_ = std::move(temporary_process_ptr_);
  FOR_EACH_OBSERVER(Observer, observer_list(), OnProcessInstanceReady());
}

void ArcBridgeService::OnSettingsInstanceReady(
    SettingsInstancePtr settings_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_settings_ptr_ = std::move(settings_ptr);
  temporary_settings_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnSettingsVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnSettingsVersionReady(int32_t version) {
  settings_ptr_ = std::move(temporary_settings_ptr_);
  FOR_EACH_OBSERVER(Observer, observer_list(), OnSettingsInstanceReady());
}

void ArcBridgeService::SetState(State state) {
  DCHECK(CalledOnValidThread());
  // DCHECK on enum classes not supported.
  DCHECK(state_ != state);
  state_ = state;
  FOR_EACH_OBSERVER(Observer, observer_list(), OnStateChanged(state_));
}

void ArcBridgeService::SetAvailable(bool available) {
  DCHECK(CalledOnValidThread());
  DCHECK(available_ != available);
  available_ = available;
  FOR_EACH_OBSERVER(Observer, observer_list(), OnAvailableChanged(available_));
}

bool ArcBridgeService::CalledOnValidThread() {
  return thread_checker_.CalledOnValidThread();
}

}  // namespace arc
