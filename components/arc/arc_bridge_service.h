// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_

#include "base/macros.h"
#include "components/arc/instance_holder.h"

namespace arc {

namespace mojom {

// Instead of including components/arc/common/arc_bridge.mojom.h, list all the
// instance classes here for faster build.
class AccessibilityHelperInstance;
class AppInstance;
class AudioInstance;
class AuthInstance;
class BluetoothInstance;
class BootPhaseMonitorInstance;
class ClipboardInstance;
class CrashCollectorInstance;
class EnterpriseReportingInstance;
class FileSystemInstance;
class ImeInstance;
class IntentHelperInstance;
class KioskInstance;
class MetricsInstance;
class NetInstance;
class NotificationsInstance;
class ObbMounterInstance;
class PolicyInstance;
class PowerInstance;
class PrintInstance;
class ProcessInstance;
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

  InstanceHolder<mojom::AccessibilityHelperInstance>* accessibility_helper() {
    return &accessibility_helper_;
  }
  InstanceHolder<mojom::AppInstance>* app() { return &app_; }
  InstanceHolder<mojom::AudioInstance>* audio() { return &audio_; }
  InstanceHolder<mojom::AuthInstance>* auth() { return &auth_; }
  InstanceHolder<mojom::BluetoothInstance>* bluetooth() { return &bluetooth_; }
  InstanceHolder<mojom::BootPhaseMonitorInstance>* boot_phase_monitor() {
    return &boot_phase_monitor_;
  }
  InstanceHolder<mojom::ClipboardInstance>* clipboard() { return &clipboard_; }
  InstanceHolder<mojom::CrashCollectorInstance>* crash_collector() {
    return &crash_collector_;
  }
  InstanceHolder<mojom::EnterpriseReportingInstance>* enterprise_reporting() {
    return &enterprise_reporting_;
  }
  InstanceHolder<mojom::FileSystemInstance>* file_system() {
    return &file_system_;
  }
  InstanceHolder<mojom::ImeInstance>* ime() { return &ime_; }
  InstanceHolder<mojom::IntentHelperInstance>* intent_helper() {
    return &intent_helper_;
  }
  InstanceHolder<mojom::KioskInstance>* kiosk() { return &kiosk_; }
  InstanceHolder<mojom::MetricsInstance>* metrics() { return &metrics_; }
  InstanceHolder<mojom::NetInstance>* net() { return &net_; }
  InstanceHolder<mojom::NotificationsInstance>* notifications() {
    return &notifications_;
  }
  InstanceHolder<mojom::ObbMounterInstance>* obb_mounter() {
    return &obb_mounter_;
  }
  InstanceHolder<mojom::PolicyInstance>* policy() { return &policy_; }
  InstanceHolder<mojom::PowerInstance>* power() { return &power_; }
  InstanceHolder<mojom::PrintInstance>* print() { return &print_; }
  InstanceHolder<mojom::ProcessInstance>* process() { return &process_; }
  InstanceHolder<mojom::StorageManagerInstance>* storage_manager() {
    return &storage_manager_;
  }
  InstanceHolder<mojom::TracingInstance>* tracing() { return &tracing_; }
  InstanceHolder<mojom::TtsInstance>* tts() { return &tts_; }
  InstanceHolder<mojom::VideoInstance>* video() { return &video_; }
  InstanceHolder<mojom::VoiceInteractionArcHomeInstance>*
  voice_interaction_arc_home() {
    return &voice_interaction_arc_home_;
  }
  InstanceHolder<mojom::VoiceInteractionFrameworkInstance>*
  voice_interaction_framework() {
    return &voice_interaction_framework_;
  }
  InstanceHolder<mojom::VolumeMounterInstance>* volume_mounter() {
    return &volume_mounter_;
  }
  InstanceHolder<mojom::WallpaperInstance>* wallpaper() { return &wallpaper_; }

 private:
  InstanceHolder<mojom::AccessibilityHelperInstance> accessibility_helper_;
  InstanceHolder<mojom::AppInstance> app_;
  InstanceHolder<mojom::AudioInstance> audio_;
  InstanceHolder<mojom::AuthInstance> auth_;
  InstanceHolder<mojom::BluetoothInstance> bluetooth_;
  InstanceHolder<mojom::BootPhaseMonitorInstance> boot_phase_monitor_;
  InstanceHolder<mojom::ClipboardInstance> clipboard_;
  InstanceHolder<mojom::CrashCollectorInstance> crash_collector_;
  InstanceHolder<mojom::EnterpriseReportingInstance> enterprise_reporting_;
  InstanceHolder<mojom::FileSystemInstance> file_system_;
  InstanceHolder<mojom::ImeInstance> ime_;
  InstanceHolder<mojom::IntentHelperInstance> intent_helper_;
  InstanceHolder<mojom::KioskInstance> kiosk_;
  InstanceHolder<mojom::MetricsInstance> metrics_;
  InstanceHolder<mojom::NetInstance> net_;
  InstanceHolder<mojom::NotificationsInstance> notifications_;
  InstanceHolder<mojom::ObbMounterInstance> obb_mounter_;
  InstanceHolder<mojom::PolicyInstance> policy_;
  InstanceHolder<mojom::PowerInstance> power_;
  InstanceHolder<mojom::PrintInstance> print_;
  InstanceHolder<mojom::ProcessInstance> process_;
  InstanceHolder<mojom::StorageManagerInstance> storage_manager_;
  InstanceHolder<mojom::TracingInstance> tracing_;
  InstanceHolder<mojom::TtsInstance> tts_;
  InstanceHolder<mojom::VideoInstance> video_;
  InstanceHolder<mojom::VoiceInteractionArcHomeInstance>
      voice_interaction_arc_home_;
  InstanceHolder<mojom::VoiceInteractionFrameworkInstance>
      voice_interaction_framework_;
  InstanceHolder<mojom::VolumeMounterInstance> volume_mounter_;
  InstanceHolder<mojom::WallpaperInstance> wallpaper_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
