// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/chrome_browser_main_linux.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chromeos/system/version_loader.h"

namespace session_manager {
class SessionManager;
}

namespace arc {
class ArcServiceManager;
}

namespace chromeos {

class ChromeInterfaceFactory;
class DataPromoNotification;
class EventRewriter;
class EventRewriterController;
class IdleActionWarningObserver;
class MagnificationManager;
class PeripheralBatteryObserver;
class PowerButtonObserver;
class PowerPrefs;
class RendererFreezer;
class SessionManagerObserver;
class SwapMetrics;
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
  scoped_ptr<default_app_order::ExternalLoader> app_order_loader_;
  scoped_ptr<PeripheralBatteryObserver> peripheral_battery_observer_;
  scoped_ptr<PowerPrefs> power_prefs_;
  scoped_ptr<PowerButtonObserver> power_button_observer_;
  scoped_ptr<IdleActionWarningObserver> idle_action_warning_observer_;
  scoped_ptr<DataPromoNotification> data_promo_notification_;
  scoped_ptr<RendererFreezer> renderer_freezer_;
  scoped_ptr<WakeOnWifiManager> wake_on_wifi_manager_;

  scoped_ptr<internal::DBusServices> dbus_services_;

  scoped_ptr<session_manager::SessionManager> session_manager_;

  scoped_ptr<EventRewriterController> keyboard_event_rewriters_;

  scoped_refptr<chromeos::ExternalMetrics> external_metrics_;

  scoped_ptr<arc::ArcServiceManager> arc_service_manager_;

#if defined(MOJO_SHELL_CLIENT)
  scoped_ptr<ChromeInterfaceFactory> interface_factory_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
