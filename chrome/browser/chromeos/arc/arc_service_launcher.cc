// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_service_launcher.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/downloads_watcher/arc_downloads_watcher_service.h"
#include "chrome/browser/chromeos/arc/enterprise/arc_enterprise_reporting_service.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_service.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_settings_service.h"
#include "chrome/browser/chromeos/arc/notification/arc_boot_error_notification.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/chromeos/arc/print/arc_print_service.h"
#include "chrome/browser/chromeos/arc/process/arc_process_service.h"
#include "chrome/browser/chromeos/arc/tts/arc_tts_service.h"
#include "chrome/browser/chromeos/arc/video/gpu_arc_video_service_host.h"
#include "chrome/browser/chromeos/arc/wallpaper/arc_wallpaper_service.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/audio/arc_audio_bridge.h"
#include "components/arc/bluetooth/arc_bluetooth_bridge.h"
#include "components/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "components/arc/clipboard/arc_clipboard_bridge.h"
#include "components/arc/crash_collector/arc_crash_collector_bridge.h"
#include "components/arc/ime/arc_ime_service.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/kiosk/arc_kiosk_bridge.h"
#include "components/arc/metrics/arc_metrics_service.h"
#include "components/arc/net/arc_net_host_impl.h"
#include "components/arc/obb_mounter/arc_obb_mounter_bridge.h"
#include "components/arc/power/arc_power_bridge.h"
#include "components/arc/storage_manager/arc_storage_manager.h"
#include "components/arc/user_data/arc_user_data_service.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/browser_thread.h"
#include "ui/arc/notification/arc_notification_manager.h"

namespace arc {
namespace {

// ChromeBrowserMainPartsChromeos owns.
ArcServiceLauncher* g_arc_service_launcher = nullptr;

}  // namespace

ArcServiceLauncher::ArcServiceLauncher() {
  DCHECK(g_arc_service_launcher == nullptr);
  g_arc_service_launcher = this;
}

ArcServiceLauncher::~ArcServiceLauncher() {
  DCHECK(!arc_service_manager_);
  DCHECK_EQ(g_arc_service_launcher, this);
  g_arc_service_launcher = nullptr;
}

// static
ArcServiceLauncher* ArcServiceLauncher::Get() {
  DCHECK(g_arc_service_launcher);
  return g_arc_service_launcher;
}

void ArcServiceLauncher::Initialize() {
  // Create ARC service manager.
  arc_service_manager_ = base::MakeUnique<ArcServiceManager>(
      content::BrowserThread::GetBlockingPool());

  ArcBridgeService* arc_bridge_service =
      arc_service_manager_->arc_bridge_service();

  // Creates ArcSessionManager at first.
  arc_session_manager_ =
      base::MakeUnique<ArcSessionManager>(arc_bridge_service);

  // List in lexicographical order.
  arc_service_manager_->AddService(
      base::MakeUnique<ArcAudioBridge>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcAuthService>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcBluetoothBridge>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcBootErrorNotification>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcBootPhaseMonitorBridge>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcClipboardBridge>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcContentFileSystemService>(arc_bridge_service));
  arc_service_manager_->AddService(base::MakeUnique<ArcCrashCollectorBridge>(
      arc_bridge_service, arc_service_manager_->blocking_task_runner()));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcDownloadsWatcherService>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcEnterpriseReportingService>(arc_bridge_service));
  auto intent_helper = base::MakeUnique<ArcIntentHelperBridge>(
      arc_bridge_service, arc_service_manager_->icon_loader(),
      arc_service_manager_->activity_resolver());
  // We don't have to remove observer since
  // ArcServiceManager always outlives ArcIntentHelperBridge.
  intent_helper->AddObserver(arc_service_manager_.get());
  arc_service_manager_->AddService(std::move(intent_helper));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcImeService>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcKioskBridge>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcMetricsService>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcNetHostImpl>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcObbMounterBridge>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcPolicyBridge>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcPowerBridge>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcPrintService>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcProcessService>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcSettingsService>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcStorageManager>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcTtsService>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<ArcWallpaperService>(arc_bridge_service));
  arc_service_manager_->AddService(
      base::MakeUnique<GpuArcVideoServiceHost>(arc_bridge_service));
}

void ArcServiceLauncher::OnPrimaryUserProfilePrepared(Profile* profile) {
  DCHECK(arc_service_manager_);
  arc_service_manager_->AddService(base::MakeUnique<ArcNotificationManager>(
      arc_service_manager_->arc_bridge_service(),
      multi_user_util::GetAccountIdFromProfile(profile)));
  arc_session_manager_->OnPrimaryUserProfilePrepared(profile);
}

void ArcServiceLauncher::Shutdown() {
  DCHECK(arc_service_manager_);
  // Destroy in the reverse order of the initialization.
  arc_service_manager_->Shutdown();
  arc_session_manager_.reset();
  arc_service_manager_.reset();
}

}  // namespace arc
