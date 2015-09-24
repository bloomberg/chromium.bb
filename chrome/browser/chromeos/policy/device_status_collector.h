// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_STATUS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_STATUS_COLLECTOR_H_

#include <deque>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/system/version_loader.h"
#include "content/public/browser/geolocation_provider.h"
#include "content/public/common/geoposition.h"
#include "policy/proto/device_management_backend.pb.h"
#include "ui/base/idle/idle.h"

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

class PrefRegistrySimple;
class PrefService;

namespace policy {

struct DeviceLocalAccount;

// Collects and summarizes the status of an enterprised-managed ChromeOS device.
class DeviceStatusCollector {
 public:
  // TODO(bartfab): Remove this once crbug.com/125931 is addressed and a proper
  // way to mock geolocation exists.
  typedef base::Callback<void(
      const content::GeolocationProvider::LocationUpdateCallback& callback)>
          LocationUpdateRequester;

  using VolumeInfoFetcher = base::Callback<
    std::vector<enterprise_management::VolumeInfo>(
        const std::vector<std::string>& mount_points)>;

  // Reads the first CPU line from /proc/stat. Returns an empty string if
  // the cpu data could not be read. Broken out into a callback to enable
  // mocking for tests.
  //
  // The format of this line from /proc/stat is:
  //   cpu  user_time nice_time system_time idle_time
  using CPUStatisticsFetcher = base::Callback<std::string(void)>;

  // Reads CPU temperatures from /sys/class/hwmon/hwmon*/temp*_input and
  // appropriate labels from /sys/class/hwmon/hwmon*/temp*_label.
  using CPUTempFetcher =
      base::Callback<std::vector<enterprise_management::CPUTempInfo>()>;

  // Constructor. Callers can inject their own VolumeInfoFetcher,
  // CPUStatisticsFetcher and CPUTempFetcher. These callbacks are executed on
  // Blocking Pool. A null callback can be passed for either parameter, to use
  // the default implementation.
  DeviceStatusCollector(
      PrefService* local_state,
      chromeos::system::StatisticsProvider* provider,
      const LocationUpdateRequester& location_update_requester,
      const VolumeInfoFetcher& volume_info_fetcher,
      const CPUStatisticsFetcher& cpu_statistics_fetcher,
      const CPUTempFetcher& cpu_temp_fetcher);
  virtual ~DeviceStatusCollector();

  // Fills in the passed proto with device status information. Will return
  // false if no status information is filled in (because status reporting
  // is disabled).
  virtual bool GetDeviceStatus(
      enterprise_management::DeviceStatusReportRequest* status);

  // Fills in the passed proto with session status information. Will return
  // false if no status information is filled in (because status reporting
  // is disabled, or because the active session is not a kiosk session).
  virtual bool GetDeviceSessionStatus(
      enterprise_management::SessionStatusReportRequest* status);

  // Called after the status information has successfully been submitted to
  // the server.
  void OnSubmittedSuccessfully();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the DeviceLocalAccount associated with the currently active
  // kiosk session, if the session was auto-launched with zero delay
  // (this enables functionality such as network reporting).
  // Virtual to allow mocking.
  virtual scoped_ptr<DeviceLocalAccount> GetAutoLaunchedKioskSessionInfo();

  // How often, in seconds, to poll to see if the user is idle.
  static const unsigned int kIdlePollIntervalSeconds = 30;

  // The total number of hardware resource usage samples cached internally.
  static const unsigned int kMaxResourceUsageSamples = 10;

 protected:
  // Check whether the user has been idle for a certain period of time.
  virtual void CheckIdleState();

  // Used instead of base::Time::Now(), to make testing possible.
  virtual base::Time GetCurrentTime();

  // Callback which receives the results of the idle state check.
  void IdleStateCallback(ui::IdleState state);

  // Gets the version of the passed app. Virtual to allow mocking.
  virtual std::string GetAppVersion(const std::string& app_id);

  // Samples the current hardware status to be sent up with the next device
  // status update.
  void SampleHardwareStatus();

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

  // Clears the cached hardware status.
  void ClearCachedHardwareStatus();

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
  void GetHardwareStatus(
      enterprise_management::DeviceStatusReportRequest* request);

  // Update the cached values of the reporting settings.
  void UpdateReportingSettings();

  void ScheduleGeolocationUpdateRequest();

  // content::GeolocationUpdateCallback implementation.
  void ReceiveGeolocationUpdate(const content::Geoposition&);

  // Callback invoked to update our cached disk information.
  void ReceiveVolumeInfo(
      const std::vector<enterprise_management::VolumeInfo>& info);

  // Callback invoked to update our cpu usage information.
  void ReceiveCPUStatistics(const std::string& statistics);

  // Callback invoked to update our CPU temp information.
  void StoreCPUTempInfo(
      const std::vector<enterprise_management::CPUTempInfo>& info);

  // Helper routine to convert from Shill-provided signal strength (percent)
  // to dBm units expected by server.
  int ConvertWifiSignalStrength(int signal_strength);

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

  base::RepeatingTimer idle_poll_timer_;
  base::RepeatingTimer hardware_status_sampling_timer_;
  base::OneShotTimer geolocation_update_timer_;

  std::string os_version_;
  std::string firmware_version_;

  content::Geoposition position_;

  // Cached disk volume information.
  std::vector<enterprise_management::VolumeInfo> volume_info_;

  // Cached CPU temp information.
  std::vector<enterprise_management::CPUTempInfo> cpu_temp_info_;

  struct ResourceUsage {
    // Sample of percentage-of-CPU-used.
    int cpu_usage_percent;

    // Amount of free RAM (measures raw memory used by processes, not internal
    // memory waiting to be reclaimed by GC).
    int64 bytes_of_ram_free;
  };

  // Samples of resource usage (contains multiple samples taken
  // periodically every kHardwareStatusSampleIntervalSeconds).
  std::deque<ResourceUsage> resource_usage_;

  // Callback invoked to fetch information about the mounted disk volumes.
  VolumeInfoFetcher volume_info_fetcher_;

  // Callback invoked to fetch information about cpu usage.
  CPUStatisticsFetcher cpu_statistics_fetcher_;

  // Callback invoked to fetch information about cpu temperature.
  CPUTempFetcher cpu_temp_fetcher_;

  chromeos::system::StatisticsProvider* statistics_provider_;

  chromeos::CrosSettings* cros_settings_;

  // The most recent CPU readings.
  uint64 last_cpu_active_;
  uint64 last_cpu_idle_;

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
  bool report_hardware_status_;
  bool report_session_status_;

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
  scoped_ptr<chromeos::CrosSettings::ObserverSubscription>
      hardware_status_subscription_;
  scoped_ptr<chromeos::CrosSettings::ObserverSubscription>
      session_status_subscription_;

  base::WeakPtrFactory<DeviceStatusCollector> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceStatusCollector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_STATUS_COLLECTOR_H_
