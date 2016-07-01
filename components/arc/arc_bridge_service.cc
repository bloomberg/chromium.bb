// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"

namespace arc {

// Weak pointer.  This class is owned by ArcServiceManager.
ArcBridgeService* g_arc_bridge_service = nullptr;

ArcBridgeService::ArcBridgeService()
    : available_(false), state_(State::STOPPED), weak_factory_(this) {
}

ArcBridgeService::~ArcBridgeService() {
  DCHECK(CalledOnValidThread());
  DCHECK(state() == State::STOPPING || state() == State::STOPPED);
}

// static
ArcBridgeService* ArcBridgeService::Get() {
  if (!g_arc_bridge_service) {
    // ArcBridgeService may be indirectly referenced in unit tests where
    // ArcBridgeService is optional.
    LOG(ERROR) << "ArcBridgeService is not ready.";
    return nullptr;
  }
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
  if (audio_instance())
    observer->OnAudioInstanceReady();
  if (auth_instance())
    observer->OnAuthInstanceReady();
  if (bluetooth_instance())
    observer->OnBluetoothInstanceReady();
  if (clipboard_instance())
    observer->OnClipboardInstanceReady();
  if (crash_collector_instance())
    observer->OnCrashCollectorInstanceReady();
  if (file_system_instance())
    observer->OnFileSystemInstanceReady();
  if (ime_instance())
    observer->OnImeInstanceReady();
  if (metrics_instance())
    observer->OnMetricsInstanceReady();
  if (net_instance())
    observer->OnNetInstanceReady();
  if (notifications_instance())
    observer->OnNotificationsInstanceReady();
  if (obb_mounter_instance())
    observer->OnObbMounterInstanceReady();
  if (policy_instance())
    observer->OnPolicyInstanceReady();
  if (power_instance())
    observer->OnPowerInstanceReady();
  if (process_instance())
    observer->OnProcessInstanceReady();
  if (storage_manager_instance())
    observer->OnStorageManagerInstanceReady();
  if (video_instance())
    observer->OnVideoInstanceReady();
  if (window_manager_instance())
    observer->OnWindowManagerInstanceReady();
}

void ArcBridgeService::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void ArcBridgeService::OnAppInstanceReady(mojom::AppInstancePtr app_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_app_ptr_ = std::move(app_ptr);
  temporary_app_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnAppVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnAppVersionReady(uint32_t version) {
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

void ArcBridgeService::OnAudioInstanceReady(mojom::AudioInstancePtr audio_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_audio_ptr_ = std::move(audio_ptr);
  temporary_audio_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnAudioVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnAudioVersionReady(uint32_t version) {
  DCHECK(CalledOnValidThread());
  audio_ptr_ = std::move(temporary_audio_ptr_);
  audio_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseAudioChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnAudioInstanceReady());
}

void ArcBridgeService::CloseAudioChannel() {
  if (!audio_ptr_)
    return;

  audio_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnAudioInstanceClosed());
}

void ArcBridgeService::OnAuthInstanceReady(mojom::AuthInstancePtr auth_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_auth_ptr_ = std::move(auth_ptr);
  temporary_auth_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnAuthVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnAuthVersionReady(uint32_t version) {
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

void ArcBridgeService::OnBluetoothInstanceReady(
    mojom::BluetoothInstancePtr bluetooth_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_bluetooth_ptr_ = std::move(bluetooth_ptr);
  temporary_bluetooth_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnBluetoothVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnBluetoothVersionReady(uint32_t version) {
  DCHECK(CalledOnValidThread());
  bluetooth_ptr_ = std::move(temporary_bluetooth_ptr_);
  bluetooth_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseBluetoothChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnBluetoothInstanceReady());
}

void ArcBridgeService::CloseBluetoothChannel() {
  DCHECK(CalledOnValidThread());
  if (!bluetooth_ptr_)
    return;

  bluetooth_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnBluetoothInstanceClosed());
}

void ArcBridgeService::OnClipboardInstanceReady(
    mojom::ClipboardInstancePtr clipboard_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_clipboard_ptr_ = std::move(clipboard_ptr);
  temporary_clipboard_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnClipboardVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnClipboardVersionReady(uint32_t version) {
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

void ArcBridgeService::OnCrashCollectorInstanceReady(
    mojom::CrashCollectorInstancePtr crash_collector_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_crash_collector_ptr_ = std::move(crash_collector_ptr);
  temporary_crash_collector_ptr_.QueryVersion(
      base::Bind(&ArcBridgeService::OnCrashCollectorVersionReady,
                 weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnCrashCollectorVersionReady(uint32_t version) {
  DCHECK(CalledOnValidThread());
  crash_collector_ptr_ = std::move(temporary_crash_collector_ptr_);
  crash_collector_ptr_.set_connection_error_handler(
      base::Bind(&ArcBridgeService::CloseCrashCollectorChannel,
                 weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnCrashCollectorInstanceReady());
}

void ArcBridgeService::CloseCrashCollectorChannel() {
  DCHECK(CalledOnValidThread());
  if (!crash_collector_ptr_)
    return;

  crash_collector_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(),
                    OnCrashCollectorInstanceClosed());
}

void ArcBridgeService::OnFileSystemInstanceReady(
    mojom::FileSystemInstancePtr file_system_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_file_system_ptr_ = std::move(file_system_ptr);
  temporary_file_system_ptr_.QueryVersion(
      base::Bind(&ArcBridgeService::OnFileSystemVersionReady,
                 weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnFileSystemVersionReady(uint32_t version) {
  DCHECK(CalledOnValidThread());
  file_system_ptr_ = std::move(temporary_file_system_ptr_);
  file_system_ptr_.set_connection_error_handler(
      base::Bind(&ArcBridgeService::CloseFileSystemChannel,
                 weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnFileSystemInstanceReady());
}

void ArcBridgeService::CloseFileSystemChannel() {
  DCHECK(CalledOnValidThread());
  if (!file_system_ptr_)
    return;

  file_system_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(),
                    OnFileSystemInstanceClosed());
}

void ArcBridgeService::OnImeInstanceReady(mojom::ImeInstancePtr ime_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_ime_ptr_ = std::move(ime_ptr);
  temporary_ime_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnImeVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnImeVersionReady(uint32_t version) {
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

void ArcBridgeService::OnIntentHelperInstanceReady(
    mojom::IntentHelperInstancePtr intent_helper_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_intent_helper_ptr_ = std::move(intent_helper_ptr);
  temporary_intent_helper_ptr_.QueryVersion(
      base::Bind(&ArcBridgeService::OnIntentHelperVersionReady,
                 weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnIntentHelperVersionReady(uint32_t version) {
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

void ArcBridgeService::OnMetricsInstanceReady(
    mojom::MetricsInstancePtr metrics_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_metrics_ptr_ = std::move(metrics_ptr);
  temporary_metrics_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnMetricsVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnMetricsVersionReady(uint32_t version) {
  DCHECK(CalledOnValidThread());
  metrics_ptr_ = std::move(temporary_metrics_ptr_);
  metrics_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseMetricsChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnMetricsInstanceReady());
}

void ArcBridgeService::CloseMetricsChannel() {
  DCHECK(CalledOnValidThread());
  if (!metrics_ptr_)
    return;

  metrics_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnMetricsInstanceClosed());
}

void ArcBridgeService::OnNetInstanceReady(mojom::NetInstancePtr net_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_net_ptr_ = std::move(net_ptr);
  temporary_net_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnNetVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnNetVersionReady(uint32_t version) {
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
    mojom::NotificationsInstancePtr notifications_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_notifications_ptr_ = std::move(notifications_ptr);
  temporary_notifications_ptr_.QueryVersion(
      base::Bind(&ArcBridgeService::OnNotificationsVersionReady,
                 weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnNotificationsVersionReady(uint32_t version) {
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

void ArcBridgeService::OnObbMounterInstanceReady(
    mojom::ObbMounterInstancePtr obb_mounter_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_obb_mounter_ptr_ = std::move(obb_mounter_ptr);
  temporary_obb_mounter_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnObbMounterVersionReady,
      weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnObbMounterVersionReady(uint32_t version) {
  DCHECK(CalledOnValidThread());
  obb_mounter_ptr_ = std::move(temporary_obb_mounter_ptr_);
  obb_mounter_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseObbMounterChannel,
      weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnObbMounterInstanceReady());
}

void ArcBridgeService::CloseObbMounterChannel() {
  if (!obb_mounter_ptr_)
    return;

  obb_mounter_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnObbMounterInstanceClosed());
}

void ArcBridgeService::OnPolicyInstanceReady(
    mojom::PolicyInstancePtr policy_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_policy_ptr_ = std::move(policy_ptr);
  temporary_policy_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnPolicyVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnPolicyVersionReady(uint32_t version) {
  DCHECK(CalledOnValidThread());
  policy_ptr_ = std::move(temporary_policy_ptr_);
  policy_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::ClosePolicyChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnPolicyInstanceReady());
}

void ArcBridgeService::ClosePolicyChannel() {
  DCHECK(CalledOnValidThread());
  if (!policy_ptr_)
    return;

  policy_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnPolicyInstanceClosed());
}

void ArcBridgeService::OnPowerInstanceReady(mojom::PowerInstancePtr power_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_power_ptr_ = std::move(power_ptr);
  temporary_power_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnPowerVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnPowerVersionReady(uint32_t version) {
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

void ArcBridgeService::OnProcessInstanceReady(
    mojom::ProcessInstancePtr process_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_process_ptr_ = std::move(process_ptr);
  temporary_process_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnProcessVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnProcessVersionReady(uint32_t version) {
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

void ArcBridgeService::OnStorageManagerInstanceReady(
    mojom::StorageManagerInstancePtr storage_manager_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_storage_manager_ptr_ = std::move(storage_manager_ptr);
  temporary_storage_manager_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnStorageManagerVersionReady,
      weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnStorageManagerVersionReady(uint32_t version) {
  DCHECK(CalledOnValidThread());
  storage_manager_ptr_ = std::move(temporary_storage_manager_ptr_);
  storage_manager_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseStorageManagerChannel,
      weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(
      Observer, observer_list(), OnStorageManagerInstanceReady());
}

void ArcBridgeService::CloseStorageManagerChannel() {
  DCHECK(CalledOnValidThread());
  if (!storage_manager_ptr_)
    return;

  storage_manager_ptr_.reset();
  FOR_EACH_OBSERVER(
      Observer, observer_list(), OnStorageManagerInstanceClosed());
}

void ArcBridgeService::OnVideoInstanceReady(mojom::VideoInstancePtr video_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_video_ptr_ = std::move(video_ptr);
  temporary_video_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnVideoVersionReady, weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnVideoVersionReady(uint32_t version) {
  DCHECK(CalledOnValidThread());
  video_ptr_ = std::move(temporary_video_ptr_);
  video_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseVideoChannel, weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnVideoInstanceReady());
}

void ArcBridgeService::CloseVideoChannel() {
  DCHECK(CalledOnValidThread());
  if (!video_ptr_)
    return;

  video_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnVideoInstanceClosed());
}

void ArcBridgeService::OnWindowManagerInstanceReady(
    mojom::WindowManagerInstancePtr window_manager_ptr) {
  DCHECK(CalledOnValidThread());
  temporary_window_manager_ptr_ = std::move(window_manager_ptr);
  temporary_window_manager_ptr_.QueryVersion(base::Bind(
      &ArcBridgeService::OnWindowManagerVersionReady,
      weak_factory_.GetWeakPtr()));
}

void ArcBridgeService::OnWindowManagerVersionReady(uint32_t version) {
  DCHECK(CalledOnValidThread());
  window_manager_ptr_ = std::move(temporary_window_manager_ptr_);
  window_manager_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeService::CloseWindowManagerChannel,
      weak_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(Observer, observer_list(), OnWindowManagerInstanceReady());
}

void ArcBridgeService::CloseWindowManagerChannel() {
  if (!window_manager_ptr_)
    return;

  window_manager_ptr_.reset();
  FOR_EACH_OBSERVER(Observer, observer_list(), OnWindowManagerInstanceClosed());
}

void ArcBridgeService::SetState(State state) {
  DCHECK(CalledOnValidThread());
  // DCHECK on enum classes not supported.
  DCHECK(state_ != state);
  state_ = state;
  VLOG(2) << "State: " << static_cast<uint32_t>(state_);
  FOR_EACH_OBSERVER(Observer, observer_list(), OnStateChanged(state_));
  if (state_ == State::READY)
    FOR_EACH_OBSERVER(Observer, observer_list(), OnBridgeReady());
  else if (state == State::STOPPED)
    FOR_EACH_OBSERVER(Observer, observer_list(), OnBridgeStopped());
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
  CloseAudioChannel();
  CloseAuthChannel();
  CloseBluetoothChannel();
  CloseClipboardChannel();
  CloseCrashCollectorChannel();
  CloseFileSystemChannel();
  CloseImeChannel();
  CloseIntentHelperChannel();
  CloseMetricsChannel();
  CloseNetChannel();
  CloseNotificationsChannel();
  CloseObbMounterChannel();
  ClosePolicyChannel();
  ClosePowerChannel();
  CloseProcessChannel();
  CloseStorageManagerChannel();
  CloseVideoChannel();
  CloseWindowManagerChannel();
}

}  // namespace arc
