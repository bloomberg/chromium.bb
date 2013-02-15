// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POWER_POWER_STATE_OVERRIDE_H_
#define CHROMEOS_POWER_POWER_STATE_OVERRIDE_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_thread_manager_observer.h"

namespace chromeos {

class DBusThreadManager;

// This class overrides the current power state on the machine, disabling
// a set of power management features.
class CHROMEOS_EXPORT PowerStateOverride
    : public base::RefCountedThreadSafe<PowerStateOverride>,
      public DBusThreadManagerObserver {
 public:
  enum Mode {
    // Blocks the screen from being dimmed or blanked due to user inactivity.
    // Also implies BLOCK_SYSTEM_SUSPEND.
    BLOCK_DISPLAY_SLEEP,

    // Blocks the system from being suspended due to user inactivity.
    BLOCK_SYSTEM_SUSPEND,
  };

  explicit PowerStateOverride(Mode mode);

  // DBusThreadManagerObserver implementation:
  virtual void OnDBusThreadManagerDestroying(DBusThreadManager* manager)
      OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<PowerStateOverride>;

  // This destructor cancels the current power state override. There might be
  // a very slight delay between the the last reference to an instance being
  // released and the power state override being canceled. This is only in
  // the case that the current instance has JUST been created and ChromeOS
  // hasn't had a chance to service the initial power override request yet.
  virtual ~PowerStateOverride();

  // Callback from RequestPowerStateOverride which receives our request_id.
  void SetRequestId(uint32 request_id);

  // Actually make a call to power manager; we need this to be able to post a
  // delayed task since we cannot call back into power manager from Heartbeat
  // since the last request has just been completed at that point.
  void CallRequestPowerStateOverrides();

  // Asks the power manager to cancel |request_id_| and sets it to zero.
  // Does nothing if it's already zero.
  void CancelRequest();

  // Bitmap containing requested override types from
  // PowerManagerClient::PowerStateOverrideType.
  uint32 override_types_;

  // Outstanding override request ID, or 0 if there is no outstanding request.
  uint32 request_id_;

  // Periodically invokes CallRequestPowerStateOverrides() to refresh the
  // override.
  base::RepeatingTimer<PowerStateOverride> heartbeat_;

  DBusThreadManager* dbus_thread_manager_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(PowerStateOverride);
};

}  // namespace chromeos

#endif  // CHROMEOS_POWER_POWER_STATE_OVERRIDE_H_
