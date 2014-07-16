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
class MagnificationManager;
class PeripheralBatteryObserver;
class PowerButtonObserver;
class PowerPrefs;
class SessionManagerObserver;
class SwapMetrics;

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
  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;

  // Stages called from PreMainMessageLoopRun.
  virtual void PreProfileInit() OVERRIDE;
  virtual void PostProfileInit() OVERRIDE;
  virtual void PreBrowserStart() OVERRIDE;
  virtual void PostBrowserStart() OVERRIDE;

  virtual void PostMainMessageLoopRun() OVERRIDE;
  virtual void PostDestroyThreads() OVERRIDE;

 private:
  scoped_ptr<default_app_order::ExternalLoader> app_order_loader_;
  scoped_ptr<ExtensionSystemEventObserver> extension_system_event_observer_;
  scoped_ptr<PeripheralBatteryObserver> peripheral_battery_observer_;
  scoped_ptr<PowerPrefs> power_prefs_;
  scoped_ptr<PowerButtonObserver> power_button_observer_;
  scoped_ptr<content::PowerSaveBlocker> retail_mode_power_save_blocker_;
  scoped_ptr<IdleActionWarningObserver> idle_action_warning_observer_;
  scoped_ptr<DataPromoNotification> data_promo_notification_;

  scoped_ptr<internal::DBusServices> dbus_services_;

  scoped_ptr<session_manager::SessionManager> session_manager_;

  scoped_ptr<EventRewriterController> keyboard_event_rewriters_;

  scoped_refptr<chromeos::ExternalMetrics> external_metrics_;

  VersionLoader cros_version_loader_;
  base::CancelableTaskTracker tracker_;
  bool use_new_network_change_notifier_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
