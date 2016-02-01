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

  // If any of the instances were ready before the call to AddObserver(), the
  // |observer| won't get any readiness events. For such cases, we have to call
  // them explicitly now to avoid a race.
  if (app_instance())
    observer->OnAppInstanceReady();
  if (auth_instance())
    observer->OnAuthInstanceReady();
  if (clipboard_instance())
    observer->OnClipboardInstanceReady();
  if (ime_instance())
    observer->OnImeInstanceReady();
  if (input_instance())
    observer->OnInputInstanceReady();
  if (net_instance())
    observer->OnNetInstanceReady();
  if (notifications_instance())
    observer->OnNotificationsInstanceReady();
  if (power_instance())
    observer->OnPowerInstanceReady();
  if (process_instance())
    observer->OnProcessInstanceReady();
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
  app_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseAppChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnAppInstanceReady());
}

void ArcBridgeService::CloseAppChannel() {
  DCHECK(CalledOnValidThread());
  if (!app_ptr_)
    return;

  app_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnAppInstanceClosed());
}

void ArcBridgeService::OnAuthInstanceReady(AuthInstancePtr auth_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_auth_ptr_ = std::move(auth_ptr);
  temporary_auth_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnAuthVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnAuthVersionReady(int32_t version) {
  DCHECK(CalledOnValidThread());
  auth_ptr_ = std::move(temporary_auth_ptr_);
  auth_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseAuthChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnAuthInstanceReady());
}

void ArcBridgeService::CloseAuthChannel() {
  DCHECK(CalledOnValidThread());
  if (!auth_ptr_)
    return;

  auth_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnAuthInstanceClosed());
}

void ArcBridgeService::OnClipboardInstanceReady(
    ClipboardInstancePtr clipboard_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_clipboard_ptr_ = std::move(clipboard_ptr);
  temporary_clipboard_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnClipboardVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnClipboardVersionReady(int32_t version) {
  DCHECK(CalledOnValidThread());
  clipboard_ptr_ = std::move(temporary_clipboard_ptr_);
  clipboard_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseClipboardChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnClipboardInstanceReady());
}

void ArcBridgeService::CloseClipboardChannel() {
  DCHECK(CalledOnValidThread());
  if (!clipboard_ptr_)
    return;

  clipboard_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnClipboardInstanceClosed());
}

void ArcBridgeService::OnImeInstanceReady(ImeInstancePtr ime_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_ime_ptr_ = std::move(ime_ptr);
  temporary_ime_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnImeVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnImeVersionReady(int32_t version) {
  DCHECK(CalledOnValidThread());
  ime_ptr_ = std::move(temporary_ime_ptr_);
  ime_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseImeChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnImeInstanceReady());
}

void ArcBridgeService::CloseImeChannel() {
  DCHECK(CalledOnValidThread());
  if (!ime_ptr_)
    return;

  ime_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnImeInstanceClosed());
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
  input_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseInputChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnInputInstanceReady());
}

void ArcBridgeService::CloseInputChannel() {
  DCHECK(CalledOnValidThread());
  if (!input_ptr_)
    return;

  input_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnInputInstanceClosed());
}

void ArcBridgeService::OnIntentHelperInstanceReady(
    IntentHelperInstancePtr intent_helper_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_intent_helper_ptr_ = std::move(intent_helper_ptr);
  temporary_intent_helper_ptr_.QueryVersion(
      base::Bind(&ArcBridgeService::OnIntentHelperVersionReady,
                 weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnIntentHelperVersionReady(int32_t version) {
  DCHECK(CalledOnValidThread());
  intent_helper_ptr_ = std::move(temporary_intent_helper_ptr_);
  intent_helper_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseIntentHelperChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnIntentHelperInstanceReady());
}

void ArcBridgeService::CloseIntentHelperChannel() {
  DCHECK(CalledOnValidThread());
  if (!intent_helper_ptr_)
    return;

  intent_helper_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnIntentHelperInstanceClosed());
}

void ArcBridgeService::OnNetInstanceReady(NetInstancePtr net_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_net_ptr_ = std::move(net_ptr);
  temporary_net_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnNetVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnNetVersionReady(int32_t version) {
  DCHECK(CalledOnValidThread());
  net_ptr_ = std::move(temporary_net_ptr_);
  net_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseNetChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnNetInstanceReady());
}

void ArcBridgeService::CloseNetChannel() {
  DCHECK(CalledOnValidThread());
  if (!net_ptr_)
    return;

  net_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnNetInstanceClosed());
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
  notifications_ptr_.set_connection_error_handler(
      base::Bind(&ArcBridgeService::CloseNotificationsChannel,
                 weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnNotificationsInstanceReady());
}

void ArcBridgeService::CloseNotificationsChannel() {
  DCHECK(CalledOnValidThread());
  if (!notifications_ptr_)
    return;

  notifications_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnNotificationsInstanceClosed());
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
  power_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::ClosePowerChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnPowerInstanceReady());
}

void ArcBridgeService::ClosePowerChannel() {
  DCHECK(CalledOnValidThread());
  if (!power_ptr_)
    return;

  power_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnPowerInstanceClosed());
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
  process_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseProcessChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnProcessInstanceReady());
}

void ArcBridgeService::CloseProcessChannel() {
  DCHECK(CalledOnValidThread());
  if (!process_ptr_)
    return;

  process_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnProcessInstanceClosed());
}

void ArcBridgeService::OnSettingsInstanceReady(
    SettingsInstancePtr settings_ptr) {
  // Obsolete interface.
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

void ArcBridgeService::CloseAllChannels() {
  // Call all the error handlers of all the channels to both close the channel
  // and notify any observers that the channel is closed.
  CloseAppChannel();
  CloseAuthChannel();
  CloseClipboardChannel();
  CloseImeChannel();
  CloseInputChannel();
  CloseIntentHelperChannel();
  CloseNetChannel();
  CloseNotificationsChannel();
  ClosePowerChannel();
  CloseProcessChannel();
}

}  // namespace arc
