// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_STATUS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_STATUS_COLLECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "chrome/browser/idle.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "content/public/browser/geolocation_provider.h"
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

class PrefRegistrySimple;
class PrefService;

namespace policy {

// Collects and summarizes the status of an enterprised-managed ChromeOS device.
class DeviceStatusCollector : public CloudPolicyClient::StatusProvider {
 public:
  // TODO(bartfab): Remove this once crbug.com/125931 is addressed and a proper
  // way to mock geolocation exists.
  typedef base::Callback<void(
      const content::GeolocationProvider::LocationUpdateCallback& callback)>
          LocationUpdateRequester;

  DeviceStatusCollector(
      PrefService* local_state,
      chromeos::system::StatisticsProvider* provider,
      LocationUpdateRequester* location_update_requester);
  virtual ~DeviceStatusCollector();

  void GetStatus(enterprise_management::DeviceStatusReportRequest* request);

  // CloudPolicyClient::StatusProvider:
  virtual bool GetDeviceStatus(
      enterprise_management::DeviceStatusReportRequest* status) OVERRIDE;
  virtual bool GetSessionStatus(
      enterprise_management::SessionStatusReportRequest* status) OVERRIDE;
  virtual void OnSubmittedSuccessfully() OVERRIDE;

  static void RegisterPrefs(PrefRegistrySimple* registry);

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

  // Trims the store activity periods to only retain data within the
  // [|min_day_key|, |max_day_key|). The record for |min_day_key| will be
  // adjusted by subtracting |min_day_trim_duration|.
  void TrimStoredActivityPeriods(int64 min_day_key,
                                 int min_day_trim_duration,
                                 int64 max_day_key);

  void AddActivePeriod(base::Time start, base::Time end);

  // Callbacks from chromeos::VersionLoader.
  void OnOSVersion(const std::string& version);
  void OnOSFirmware(const std::string& version);

  // Helpers for the various portions of the status.
  void GetActivityTimes(
      enterprise_management::DeviceStatusReportRequest* request);
  void GetVersionInfo(
      enterprise_management::DeviceStatusReportRequest* request);
  void GetBootMode(
      enterprise_management::DeviceStatusReportRequest* request);
  void GetLocation(
      enterprise_management::DeviceStatusReportRequest* request);
  void GetNetworkInterfaces(
      enterprise_management::DeviceStatusReportRequest* request);
  void GetUsers(
      enterprise_management::DeviceStatusReportRequest* request);

  // Update the cached values of the reporting settings.
  void UpdateReportingSettings();

  void ScheduleGeolocationUpdateRequest();

  // content::GeolocationUpdateCallback implementation.
  void ReceiveGeolocationUpdate(const content::Geoposition&);

  // How often to poll to see if the user is idle.
  int poll_interval_seconds_;

  PrefService* local_state_;

  // The last time an idle state check was performed.
  base::Time last_idle_check_;

  // The maximum key that went into the last report generated by
  // GetDeviceStatus(), and the duration for it. This is used to trim the
  // stored data in OnSubmittedSuccessfully(). Trimming is delayed so
  // unsuccessful uploads don't result in dropped data.
  int64 last_reported_day_;
  int duration_for_last_reported_day_;

  // Whether a geolocation update is currently in progress.
  bool geolocation_update_in_progress_;

  base::RepeatingTimer<DeviceStatusCollector> idle_poll_timer_;
  base::OneShotTimer<DeviceStatusCollector> geolocation_update_timer_;

  chromeos::VersionLoader version_loader_;
  base::CancelableTaskTracker tracker_;

  std::string os_version_;
  std::string firmware_version_;

  content::Geoposition position_;

  chromeos::system::StatisticsProvider* statistics_provider_;

  chromeos::CrosSettings* cros_settings_;

  base::WeakPtrFactory<DeviceStatusCollector> weak_factory_;

  // TODO(bartfab): Remove this once crbug.com/125931 is addressed and a proper
  // way to mock geolocation exists.
  LocationUpdateRequester location_update_requester_;

  scoped_ptr<content::GeolocationProvider::Subscription>
      geolocation_subscription_;

  // Cached values of the reporting settings from the device policy.
  bool report_version_info_;
  bool report_activity_times_;
  bool report_boot_mode_;
  bool report_location_;
  bool report_network_interfaces_;
  bool report_users_;

  scoped_ptr<chromeos::CrosSettings::ObserverSubscription>
      version_info_subscription_;
  scoped_ptr<chromeos::CrosSettings::ObserverSubscription>
      activity_times_subscription_;
  scoped_ptr<chromeos::CrosSettings::ObserverSubscription>
      boot_mode_subscription_;
  scoped_ptr<chromeos::CrosSettings::ObserverSubscription>
      location_subscription_;
  scoped_ptr<chromeos::CrosSettings::ObserverSubscription>
      network_interfaces_subscription_;
  scoped_ptr<chromeos::CrosSettings::ObserverSubscription>
      users_subscription_;

  DISALLOW_COPY_AND_ASSIGN(DeviceStatusCollector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_STATUS_COLLECTOR_H_
