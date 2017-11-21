// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_

#include "base/macros.h"
#include "components/arc/connection_holder.h"

namespace arc {

namespace mojom {

// Instead of including components/arc/common/arc_bridge.mojom.h, list all the
// instance classes here for faster build.
class AccessibilityHelperInstance;
class AppHost;
class AppInstance;
class AudioInstance;
class AuthInstance;
class BackupSettingsInstance;
class BluetoothInstance;
class BootPhaseMonitorInstance;
class CastReceiverInstance;
class CertStoreInstance;
class ClipboardInstance;
class CrashCollectorInstance;
class EnterpriseReportingInstance;
class FileSystemInstance;
class ImeInstance;
class IntentHelperInstance;
class KioskInstance;
class LockScreenInstance;
class MetricsInstance;
class MidisInstance;
class NetInstance;
class NotificationsInstance;
class ObbMounterInstance;
class OemCryptoInstance;
class PolicyInstance;
class PowerInstance;
class PrintInstance;
class ProcessInstance;
class RotationLockInstance;
class StorageManagerInstance;
class TracingInstance;
class TtsInstance;
class VideoInstance;
class VoiceInteractionArcHomeInstance;
class VoiceInteractionFrameworkInstance;
class VolumeMounterInstance;
class WallpaperInstance;

}  // namespace mojom

// Holds Mojo channels which proxy to ARC side implementation. The actual
// instances are set/removed via ArcBridgeHostImpl.
class ArcBridgeService {
 public:
  ArcBridgeService();
  ~ArcBridgeService();

  ConnectionHolder<mojom::AccessibilityHelperInstance>* accessibility_helper() {
    return &accessibility_helper_;
  }
  ConnectionHolder<mojom::AppInstance, mojom::AppHost>* app() { return &app_; }
  ConnectionHolder<mojom::AudioInstance>* audio() { return &audio_; }
  ConnectionHolder<mojom::AuthInstance>* auth() { return &auth_; }
  ConnectionHolder<mojom::BackupSettingsInstance>* backup_settings() {
    return &backup_settings_;
  }
  ConnectionHolder<mojom::BluetoothInstance>* bluetooth() {
    return &bluetooth_;
  }
  ConnectionHolder<mojom::BootPhaseMonitorInstance>* boot_phase_monitor() {
    return &boot_phase_monitor_;
  }
  ConnectionHolder<mojom::CastReceiverInstance>* cast_receiver() {
    return &cast_receiver_;
  }
  ConnectionHolder<mojom::CertStoreInstance>* cert_store() {
    return &cert_store_;
  }
  ConnectionHolder<mojom::ClipboardInstance>* clipboard() {
    return &clipboard_;
  }
  ConnectionHolder<mojom::CrashCollectorInstance>* crash_collector() {
    return &crash_collector_;
  }
  ConnectionHolder<mojom::EnterpriseReportingInstance>* enterprise_reporting() {
    return &enterprise_reporting_;
  }
  ConnectionHolder<mojom::FileSystemInstance>* file_system() {
    return &file_system_;
  }
  ConnectionHolder<mojom::ImeInstance>* ime() { return &ime_; }
  ConnectionHolder<mojom::IntentHelperInstance>* intent_helper() {
    return &intent_helper_;
  }
  ConnectionHolder<mojom::KioskInstance>* kiosk() { return &kiosk_; }
  ConnectionHolder<mojom::LockScreenInstance>* lock_screen() {
    return &lock_screen_;
  }
  ConnectionHolder<mojom::MetricsInstance>* metrics() { return &metrics_; }
  ConnectionHolder<mojom::MidisInstance>* midis() { return &midis_; }
  ConnectionHolder<mojom::NetInstance>* net() { return &net_; }
  ConnectionHolder<mojom::NotificationsInstance>* notifications() {
    return &notifications_;
  }
  ConnectionHolder<mojom::ObbMounterInstance>* obb_mounter() {
    return &obb_mounter_;
  }
  ConnectionHolder<mojom::OemCryptoInstance>* oemcrypto() {
    return &oemcrypto_;
  }
  ConnectionHolder<mojom::PolicyInstance>* policy() { return &policy_; }
  ConnectionHolder<mojom::PowerInstance>* power() { return &power_; }
  ConnectionHolder<mojom::PrintInstance>* print() { return &print_; }
  ConnectionHolder<mojom::ProcessInstance>* process() { return &process_; }
  ConnectionHolder<mojom::RotationLockInstance>* rotation_lock() {
    return &rotation_lock_;
  }
  ConnectionHolder<mojom::StorageManagerInstance>* storage_manager() {
    return &storage_manager_;
  }
  ConnectionHolder<mojom::TracingInstance>* tracing() { return &tracing_; }
  ConnectionHolder<mojom::TtsInstance>* tts() { return &tts_; }
  ConnectionHolder<mojom::VideoInstance>* video() { return &video_; }
  ConnectionHolder<mojom::VoiceInteractionArcHomeInstance>*
  voice_interaction_arc_home() {
    return &voice_interaction_arc_home_;
  }
  ConnectionHolder<mojom::VoiceInteractionFrameworkInstance>*
  voice_interaction_framework() {
    return &voice_interaction_framework_;
  }
  ConnectionHolder<mojom::VolumeMounterInstance>* volume_mounter() {
    return &volume_mounter_;
  }
  ConnectionHolder<mojom::WallpaperInstance>* wallpaper() {
    return &wallpaper_;
  }

 private:
  ConnectionHolder<mojom::AccessibilityHelperInstance> accessibility_helper_;
  ConnectionHolder<mojom::AppInstance, mojom::AppHost> app_;
  ConnectionHolder<mojom::AudioInstance> audio_;
  ConnectionHolder<mojom::AuthInstance> auth_;
  ConnectionHolder<mojom::BackupSettingsInstance> backup_settings_;
  ConnectionHolder<mojom::BluetoothInstance> bluetooth_;
  ConnectionHolder<mojom::BootPhaseMonitorInstance> boot_phase_monitor_;
  ConnectionHolder<mojom::CastReceiverInstance> cast_receiver_;
  ConnectionHolder<mojom::CertStoreInstance> cert_store_;
  ConnectionHolder<mojom::ClipboardInstance> clipboard_;
  ConnectionHolder<mojom::CrashCollectorInstance> crash_collector_;
  ConnectionHolder<mojom::EnterpriseReportingInstance> enterprise_reporting_;
  ConnectionHolder<mojom::FileSystemInstance> file_system_;
  ConnectionHolder<mojom::ImeInstance> ime_;
  ConnectionHolder<mojom::IntentHelperInstance> intent_helper_;
  ConnectionHolder<mojom::KioskInstance> kiosk_;
  ConnectionHolder<mojom::LockScreenInstance> lock_screen_;
  ConnectionHolder<mojom::MetricsInstance> metrics_;
  ConnectionHolder<mojom::MidisInstance> midis_;
  ConnectionHolder<mojom::NetInstance> net_;
  ConnectionHolder<mojom::NotificationsInstance> notifications_;
  ConnectionHolder<mojom::ObbMounterInstance> obb_mounter_;
  ConnectionHolder<mojom::OemCryptoInstance> oemcrypto_;
  ConnectionHolder<mojom::PolicyInstance> policy_;
  ConnectionHolder<mojom::PowerInstance> power_;
  ConnectionHolder<mojom::PrintInstance> print_;
  ConnectionHolder<mojom::ProcessInstance> process_;
  ConnectionHolder<mojom::RotationLockInstance> rotation_lock_;
  ConnectionHolder<mojom::StorageManagerInstance> storage_manager_;
  ConnectionHolder<mojom::TracingInstance> tracing_;
  ConnectionHolder<mojom::TtsInstance> tts_;
  ConnectionHolder<mojom::VideoInstance> video_;
  ConnectionHolder<mojom::VoiceInteractionArcHomeInstance>
      voice_interaction_arc_home_;
  ConnectionHolder<mojom::VoiceInteractionFrameworkInstance>
      voice_interaction_framework_;
  ConnectionHolder<mojom::VolumeMounterInstance> volume_mounter_;
  ConnectionHolder<mojom::WallpaperInstance> wallpaper_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
