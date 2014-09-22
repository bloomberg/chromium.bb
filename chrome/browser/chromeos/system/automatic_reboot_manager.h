// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_AUTOMATIC_REBOOT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_AUTOMATIC_REBOOT_MANAGER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/wm/core/user_activity_observer.h"

class PrefRegistrySimple;

namespace base {
class TickClock;
}

namespace chromeos {
namespace system {

class AutomaticRebootManagerObserver;

// Schedules and executes automatic reboots.
//
// Automatic reboots may be scheduled for any number of reasons. Currently, the
// following are implemented:
// * When Chrome OS has applied a system update, a reboot may become necessary
//   to complete the update process. If the policy to automatically reboot after
//   an update is enabled, a reboot is scheduled at that point.
// * If an uptime limit is set through policy, a reboot is scheduled when the
//   device's uptime reaches the limit. Time spent sleeping counts as uptime as
//   well.
//
// When the time of the earliest scheduled reboot is reached, the reboot is
// requested. The reboot is performed immediately unless one of the following
// reasons inhibits it:
// * If the login screen is being shown: Reboots are inhibited while the user is
//   interacting with the screen (determined by checking whether there has been
//   any user activity in the past 60 seconds).
// * If a session is in progress: Reboots are inhibited until the session ends,
//   the browser is restarted or the device is suspended.
//
// If reboots are inhibited, a 24 hour grace period is started. The reboot
// request is carried out the moment none of the inhibiting criteria apply
// anymore (e.g. the user becomes idle on the login screen, the user logs exits
// a session, the user suspends the device). If reboots remain inhibited for the
// entire grace period, a reboot is unconditionally performed at its end.
//
// Note: Currently, automatic reboots are only enabled while the login screen is
// being shown or a kiosk app session is in progress. This will change in the
// future and the policy will always apply, regardless of whether a session of
// any particular type is in progress or not. http://crbug.com/244972
//
// Reboots may be scheduled and canceled at any time. This causes the time at
// which a reboot should be requested and the grace period that follows it to
// be recalculated.
//
// Reboots are scheduled in terms of device uptime. The current uptime is read
// from /proc/uptime. The time at which a reboot became necessary to finish
// applying an update is stored in /var/run/chrome/update_reboot_needed_uptime,
// making it persist across browser restarts and crashes. Placing the file under
// /var/run ensures that it gets cleared automatically on every boot.
class AutomaticRebootManager : public PowerManagerClient::Observer,
                               public UpdateEngineClient::Observer,
                               public wm::UserActivityObserver,
                               public content::NotificationObserver {
 public:
  // The current uptime and the uptime at which an update was applied and a
  // reboot became necessary (if any). Used to pass this information from the
  // blocking thread pool to the UI thread.
  struct SystemEventTimes {
    SystemEventTimes();
    SystemEventTimes(const base::TimeDelta& uptime,
                     const base::TimeDelta& update_reboot_needed_uptime);

    bool has_boot_time;
    base::TimeTicks boot_time;

    bool has_update_reboot_needed_time;
    base::TimeTicks update_reboot_needed_time;
  };

  explicit AutomaticRebootManager(scoped_ptr<base::TickClock> clock);
  virtual ~AutomaticRebootManager();

  void AddObserver(AutomaticRebootManagerObserver* observer);
  void RemoveObserver(AutomaticRebootManagerObserver* observer);

  // PowerManagerClient::Observer:
  virtual void SuspendDone(const base::TimeDelta& sleep_duration) OVERRIDE;

  // UpdateEngineClient::Observer:
  virtual void UpdateStatusChanged(
      const UpdateEngineClient::Status& status) OVERRIDE;

  // wm::UserActivityObserver:
  virtual void OnUserActivity(const ui::Event* event) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  friend class AutomaticRebootManagerBasicTest;

  // Finishes initialization. Called after the |system_event_times| have been
  // loaded in the blocking thread pool.
  void Init(const SystemEventTimes& system_event_times);

  // Reschedules the reboot request, start and end of the grace period. Reboots
  // immediately if the end of the grace period has already passed.
  void Reschedule();

  // Requests a reboot.
  void RequestReboot();

  // Called whenever the status of the criteria inhibiting reboots may have
  // changed. Reboots immediately if a reboot has actually been requested and
  // none of the criteria inhibiting it apply anymore. Otherwise, does nothing.
  // If |ignore_session|, a session in progress does not inhibit reboots.
  void MaybeReboot(bool ignore_session);

  // Reboots immediately.
  void Reboot();

  // A clock that can be mocked in tests to fast-forward time.
  scoped_ptr<base::TickClock> clock_;

  PrefChangeRegistrar local_state_registrar_;

  content::NotificationRegistrar notification_registrar_;

  // Fires when the user has been idle on the login screen for a set amount of
  // time.
  scoped_ptr<base::OneShotTimer<AutomaticRebootManager> >
      login_screen_idle_timer_;

  // The time at which the device was booted, in |clock_| ticks.
  bool have_boot_time_;
  base::TimeTicks boot_time_;

  // The time at which an update was applied and a reboot became necessary to
  // complete the update process, in |clock_| ticks.
  bool have_update_reboot_needed_time_;
  base::TimeTicks update_reboot_needed_time_;

  // Whether a reboot has been requested.
  bool reboot_requested_;

  // Timers that start and end the grace period.
  scoped_ptr<base::OneShotTimer<AutomaticRebootManager> > grace_start_timer_;
  scoped_ptr<base::OneShotTimer<AutomaticRebootManager> > grace_end_timer_;

  ObserverList<AutomaticRebootManagerObserver, true> observers_;

  base::WeakPtrFactory<AutomaticRebootManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutomaticRebootManager);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_AUTOMATIC_REBOOT_MANAGER_H_
