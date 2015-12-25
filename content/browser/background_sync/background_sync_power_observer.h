// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_POWER_OBSERVER_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_POWER_OBSERVER_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/power_monitor/power_observer.h"
#include "content/browser/background_sync/background_sync.pb.h"
#include "content/common/content_export.h"

namespace content {

// BackgroundSyncPowerObserver monitors the charging status of the device and
// determines if the power conditions of BackgroundSyncRegistrations are met.
class CONTENT_EXPORT BackgroundSyncPowerObserver : public base::PowerObserver {
 public:
  // Creates a BackgroundSyncPowerObserver. |power_callback| is run when the
  // battery states changes asynchronously via PostMessage.
  explicit BackgroundSyncPowerObserver(const base::Closure& power_callback);

  ~BackgroundSyncPowerObserver() override;

  // Returns true if the state of the battery (charging or not) meets the needs
  // of |power_state|.
  bool PowerSufficient(SyncPowerState power_state) const;

 private:
  friend class BackgroundSyncPowerObserverTest;

  // PowerObserver overrides
  void OnPowerStateChange(bool on_battery_power) override;

  // |observing_power_monitor_| is true when the constructor is able to find and
  // register an observer with the base::PowerMonitor. This should always be
  // true except for tests in which the browser initialization isn't done.
  bool observing_power_monitor_;

  bool on_battery_;

  // The callback to run when the battery state changes.
  base::Closure power_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncPowerObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_POWER_OBSERVER_H_
