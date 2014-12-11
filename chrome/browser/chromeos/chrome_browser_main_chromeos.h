// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_

#include "base/memory/scoped_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/chrome_browser_main_linux.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/chromeos/version_loader.h"

namespace base {
class MemoryPressureObserverChromeOS;
}

namespace content {
class PowerSaveBlocker;
}

namespace session_manager {
class SessionManager;
}

namespace chromeos {

class DataPromoNotification;
class EventRewriter;
class EventRewriterController;
class ExtensionSystemEventObserver;
class IdleActionWarningObserver;
class LightBar;
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
  virtual ~ChromeBrowserMainPartsChromeos();

  // ChromeBrowserMainParts overrides.
  virtual void PreEarlyInitialization() override;
  virtual void PreMainMessageLoopStart() override;
  virtual void PostMainMessageLoopStart() override;
  virtual void PreMainMessageLoopRun() override;

  // Stages called from PreMainMessageLoopRun.
  virtual void PreProfileInit() override;
  virtual void PostProfileInit() override;
  virtual void PreBrowserStart() override;
  virtual void PostBrowserStart() override;

  virtual void PostMainMessageLoopRun() override;
  virtual void PostDestroyThreads() override;

 private:
  scoped_ptr<default_app_order::ExternalLoader> app_order_loader_;
  scoped_ptr<ExtensionSystemEventObserver> extension_system_event_observer_;
  scoped_ptr<PeripheralBatteryObserver> peripheral_battery_observer_;
  scoped_ptr<PowerPrefs> power_prefs_;
  scoped_ptr<PowerButtonObserver> power_button_observer_;
  scoped_ptr<content::PowerSaveBlocker> retail_mode_power_save_blocker_;
  scoped_ptr<IdleActionWarningObserver> idle_action_warning_observer_;
  scoped_ptr<DataPromoNotification> data_promo_notification_;
  scoped_ptr<RendererFreezer> renderer_freezer_;
  scoped_ptr<LightBar> light_bar_;
  scoped_ptr<WakeOnWifiManager> wake_on_wifi_manager_;

  scoped_ptr<internal::DBusServices> dbus_services_;

  scoped_ptr<session_manager::SessionManager> session_manager_;

  scoped_ptr<EventRewriterController> keyboard_event_rewriters_;

  scoped_refptr<chromeos::ExternalMetrics> external_metrics_;

  scoped_ptr<base::MemoryPressureObserverChromeOS> memory_pressure_observer_;

  bool use_new_network_change_notifier_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
