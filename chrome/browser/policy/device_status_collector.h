// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_STATUS_COLLECTOR_H_
#define CHROME_BROWSER_POLICY_DEVICE_STATUS_COLLECTOR_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "chrome/browser/idle.h"
#include "content/public/browser/geolocation.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/common/geoposition.h"

namespace chromeos {
class CrosSettings;
namespace system {
class StatisticsProvider;
}
}

namespace content {
class NotificationDetails;
class NotificationSource;
}

namespace enterprise_management {
class DeviceStatusReportRequest;
}

class PrefService;

namespace policy {

// Collects and summarizes the status of an enterprised-managed ChromeOS device.
class DeviceStatusCollector : public content::NotificationObserver {
 public:
  // TODO(bartfab): Remove this once crbug.com/125931 is addressed and a proper
  // way to mock geolocation exists.
  typedef void(*LocationUpdateRequester)(
      const content::GeolocationUpdateCallback& callback);

  DeviceStatusCollector(PrefService* local_state,
                        chromeos::system::StatisticsProvider* provider,
                        LocationUpdateRequester location_update_requester);
  virtual ~DeviceStatusCollector();

  void GetStatus(enterprise_management::DeviceStatusReportRequest* request);

  static void RegisterPrefs(PrefService* local_state);

  // How often, in seconds, to poll to see if the user is idle.
  static const unsigned int kIdlePollIntervalSeconds = 30;

 protected:
  // Check whether the user has been idle for a certain period of time.
  virtual void CheckIdleState();

  // Used instead of base::Time::Now(), to make testing possible.
  virtual base::Time GetCurrentTime();

  // Callback which receives the results of the idle state check.
  void IdleStateCallback(IdleState state);

  // The number of days in the past to store device activity.
  // This is kept in case device status uploads fail for a number of days.
  unsigned int max_stored_past_activity_days_;

  // The number of days in the future to store device activity.
  // When changing the system time and/or timezones, it's possible to record
  // activity time that is slightly in the future.
  unsigned int max_stored_future_activity_days_;

 private:
  // Prevents the local store of activity periods from growing too large by
  // removing entries that are outside the reporting window.
  void PruneStoredActivityPeriods(base::Time base_time);

  void AddActivePeriod(base::Time start, base::Time end);

  // Callbacks from chromeos::VersionLoader.
  void OnOSVersion(chromeos::VersionLoader::Handle handle,
                   std::string version);
  void OnOSFirmware(chromeos::VersionLoader::Handle handle,
                    std::string version);

  // Helpers for the various portions of the status.
  void GetActivityTimes(
      enterprise_management::DeviceStatusReportRequest* request);
  void GetVersionInfo(
      enterprise_management::DeviceStatusReportRequest* request);
  void GetBootMode(
      enterprise_management::DeviceStatusReportRequest* request);
  void GetLocation(
      enterprise_management::DeviceStatusReportRequest* request);

  // Update the cached values of the reporting settings.
  void UpdateReportingSettings();

  // content::NotificationObserver interface.
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

  void ScheduleGeolocationUpdateRequest();

  // content::GeolocationUpdateCallback implementation.
  void ReceiveGeolocationUpdate(const content::Geoposition&);

  // How often to poll to see if the user is idle.
  int poll_interval_seconds_;

  PrefService* local_state_;

  // The last time an idle state check was performed.
  base::Time last_idle_check_;

  // Whether a geolocation update is currently in progress.
  bool geolocation_update_in_progress_;

  base::RepeatingTimer<DeviceStatusCollector> idle_poll_timer_;
  base::OneShotTimer<DeviceStatusCollector> geolocation_update_timer_;

  chromeos::VersionLoader version_loader_;
  CancelableRequestConsumer consumer_;

  std::string os_version_;
  std::string firmware_version_;

  content::Geoposition position_;

  chromeos::system::StatisticsProvider* statistics_provider_;

  chromeos::CrosSettings* cros_settings_;

  base::WeakPtrFactory<DeviceStatusCollector> weak_factory_;

  // TODO(bartfab): Remove this once crbug.com/125931 is addressed and a proper
  // way to mock geolocation exists.
  LocationUpdateRequester location_update_requester_;

  // Cached values of the reporting settings from the device policy.
  bool report_version_info_;
  bool report_activity_times_;
  bool report_boot_mode_;
  bool report_location_;

  DISALLOW_COPY_AND_ASSIGN(DeviceStatusCollector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_STATUS_COLLECTOR_H_
