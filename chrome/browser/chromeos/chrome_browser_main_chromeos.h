// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_

#include <memory>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/chrome_browser_main_linux.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chromeos/system/version_loader.h"

namespace session_manager {
class SessionManager;
}

namespace arc {
class ArcServiceLauncher;
}

namespace chromeos {

class ArcKioskAppManager;
class DataPromoNotification;
class EventRewriterController;
class ExtensionVolumeObserver;
class IdleActionWarningObserver;
class LoginLockStateNotifier;
class LowDiskNotification;
class NetworkPrefStateObserver;
class NetworkThrottlingObserver;
class PeripheralBatteryObserver;
class PowerPrefs;
class RendererFreezer;
class ShutdownPolicyForwarder;
class WakeOnWifiManager;

namespace default_app_order {
class ExternalLoader;
}

namespace internal {
class DBusServices;
}

class ChromeBrowserMainPartsChromeos : public ChromeBrowserMainPartsLinux {
 public:
  explicit ChromeBrowserMainPartsChromeos(
      const content::MainFunctionParams& parameters);
  ~ChromeBrowserMainPartsChromeos() override;

  // ChromeBrowserMainParts overrides.
  void PreEarlyInitialization() override;
  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  void PreMainMessageLoopRun() override;

  // Stages called from PreMainMessageLoopRun.
  void PreProfileInit() override;
  void PostProfileInit() override;
  void PreBrowserStart() override;
  void PostBrowserStart() override;

  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

 private:
  std::unique_ptr<default_app_order::ExternalLoader> app_order_loader_;
  std::unique_ptr<NetworkPrefStateObserver> network_pref_state_observer_;
  std::unique_ptr<ExtensionVolumeObserver> extension_volume_observer_;
  std::unique_ptr<PeripheralBatteryObserver> peripheral_battery_observer_;
  std::unique_ptr<PowerPrefs> power_prefs_;
  std::unique_ptr<LoginLockStateNotifier> login_lock_state_notifier_;
  std::unique_ptr<IdleActionWarningObserver> idle_action_warning_observer_;
  std::unique_ptr<DataPromoNotification> data_promo_notification_;
  std::unique_ptr<RendererFreezer> renderer_freezer_;
  std::unique_ptr<WakeOnWifiManager> wake_on_wifi_manager_;
  std::unique_ptr<NetworkThrottlingObserver> network_throttling_observer_;

  std::unique_ptr<internal::DBusServices> dbus_services_;

  std::unique_ptr<session_manager::SessionManager> session_manager_;

  std::unique_ptr<ShutdownPolicyForwarder> shutdown_policy_forwarder_;

  std::unique_ptr<EventRewriterController> keyboard_event_rewriters_;

  scoped_refptr<chromeos::ExternalMetrics> external_metrics_;

  std::unique_ptr<arc::ArcServiceLauncher> arc_service_launcher_;

  std::unique_ptr<LowDiskNotification> low_disk_notification_;
  std::unique_ptr<ArcKioskAppManager> arc_kiosk_app_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
