// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_status_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <sstream>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/enterprise_reporting.mojom.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using base::Time;
using base::TimeDelta;

namespace em = enterprise_management;

namespace {
// How many seconds of inactivity triggers the idle state.
const int kIdleStateThresholdSeconds = 300;

// How many days in the past to store active periods for.
const unsigned int kMaxStoredPastActivityDays = 30;

// How many days in the future to store active periods for.
const unsigned int kMaxStoredFutureActivityDays = 2;

// How often, in seconds, to sample the hardware resource usage.
const unsigned int kResourceUsageSampleIntervalSeconds = 120;

// The location we read our CPU statistics from.
const char kProcStat[] = "/proc/stat";

// The location we read our CPU temperature and channel label from.
const char kHwmonDir[] = "/sys/class/hwmon/";
const char kDeviceDir[] = "device";
const char kHwmonDirectoryPattern[] = "hwmon*";
const char kCPUTempFilePattern[] = "temp*_input";

// Determine the day key (milliseconds since epoch for corresponding day in UTC)
// for a given |timestamp|.
int64_t TimestampToDayKey(Time timestamp) {
  Time::Exploded exploded;
  timestamp.LocalMidnight().LocalExplode(&exploded);
  Time out_time;
  bool conversion_success = Time::FromUTCExploded(exploded, &out_time);
  DCHECK(conversion_success);
  return out_time.ToJavaTime();
}

// Helper function (invoked via blocking pool) to fetch information about
// mounted disks.
std::vector<em::VolumeInfo> GetVolumeInfo(
    const std::vector<std::string>& mount_points) {
  std::vector<em::VolumeInfo> result;
  for (const std::string& mount_point : mount_points) {
    base::FilePath mount_path(mount_point);

    // Non-native file systems do not have a mount point in the local file
    // system. However, it's worth checking here, as it's easier than checking
    // earlier which mount point is local, and which one is not.
    if (mount_point.empty() || !base::PathExists(mount_path))
      continue;

    int64_t free_size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
    int64_t total_size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
    if (free_size < 0 || total_size < 0) {
      LOG(ERROR) << "Unable to get volume status for " << mount_point;
      continue;
    }
    em::VolumeInfo info;
    info.set_volume_id(mount_point);
    info.set_storage_total(total_size);
    info.set_storage_free(free_size);
    result.push_back(info);
  }
  return result;
}

// Reads the first CPU line from /proc/stat. Returns an empty string if
// the cpu data could not be read.
// The format of this line from /proc/stat is:
//
//   cpu  user_time nice_time system_time idle_time
//
// where user_time, nice_time, system_time, and idle_time are all integer
// values measured in jiffies from system startup.
std::string ReadCPUStatistics() {
  std::string contents;
  if (base::ReadFileToString(base::FilePath(kProcStat), &contents)) {
    size_t eol = contents.find("\n");
    if (eol != std::string::npos) {
      std::string line = contents.substr(0, eol);
      if (line.compare(0, 4, "cpu ") == 0)
        return line;
    }
    // First line should always start with "cpu ".
    NOTREACHED() << "Could not parse /proc/stat contents: " << contents;
  }
  LOG(WARNING) << "Unable to read CPU statistics from " << kProcStat;
  return std::string();
}

// Read system temperature sensors from
//
// /sys/class/hwmon/hwmon*/(device/)?temp*_input
//
// which contains millidegree Celsius temperature and
//
// /sys/class/hwmon/hwmon*/(device/)?temp*_label or
// /sys/class/hwmon/hwmon*/name
//
// which contains an appropriate label name for the given sensor.
std::vector<em::CPUTempInfo> ReadCPUTempInfo() {
  std::vector<em::CPUTempInfo> contents;
  // Get directories /sys/class/hwmon/hwmon*
  base::FileEnumerator hwmon_enumerator(base::FilePath(kHwmonDir), false,
                                        base::FileEnumerator::DIRECTORIES,
                                        kHwmonDirectoryPattern);

  for (base::FilePath hwmon_path = hwmon_enumerator.Next(); !hwmon_path.empty();
       hwmon_path = hwmon_enumerator.Next()) {
    // Get temp*_input files in hwmon*/ and hwmon*/device/
    if (base::PathExists(hwmon_path.Append(kDeviceDir))) {
      hwmon_path = hwmon_path.Append(kDeviceDir);
    }
    base::FileEnumerator enumerator(
        hwmon_path, false, base::FileEnumerator::FILES, kCPUTempFilePattern);
    for (base::FilePath temperature_path = enumerator.Next();
         !temperature_path.empty(); temperature_path = enumerator.Next()) {
      // Get appropriate temp*_label file.
      std::string label_path = temperature_path.MaybeAsASCII();
      if (label_path.empty()) {
        LOG(WARNING) << "Unable to parse a path to temp*_input file as ASCII";
        continue;
      }
      base::ReplaceSubstringsAfterOffset(&label_path, 0, "input", "label");
      base::FilePath name_path = hwmon_path.Append("name");

      // Get the label describing this temperature. Use temp*_label
      // if present, fall back on name file or blank.
      std::string label;
      if (base::PathExists(base::FilePath(label_path))) {
        base::ReadFileToString(base::FilePath(label_path), &label);
      } else if (base::PathExists(base::FilePath(name_path))) {
        base::ReadFileToString(name_path, &label);
      } else {
        label = std::string();
      }

      // Read temperature in millidegree Celsius.
      std::string temperature_string;
      int32_t temperature = 0;
      if (base::ReadFileToString(temperature_path, &temperature_string) &&
          sscanf(temperature_string.c_str(), "%d", &temperature) == 1) {
        // CPU temp in millidegree Celsius to Celsius
        temperature /= 1000;

        em::CPUTempInfo info;
        info.set_cpu_label(label);
        info.set_cpu_temp(temperature);
        contents.push_back(info);
      } else {
        LOG(WARNING) << "Unable to read CPU temp from "
                     << temperature_path.MaybeAsASCII();
      }
    }
  }
  return contents;
}

bool ReadAndroidStatus(
    const policy::DeviceStatusCollector::AndroidStatusReceiver& receiver) {
  auto* const arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;
  auto* const instance_holder =
      arc_service_manager->arc_bridge_service()->enterprise_reporting();
  if (!instance_holder)
    return false;
  auto* const instance =
      ARC_GET_INSTANCE_FOR_METHOD(instance_holder, GetStatus);
  if (!instance)
    return false;
  instance->GetStatus(receiver);
  return true;
}

// Returns the DeviceLocalAccount associated with the current kiosk session.
// Returns null if there is no active kiosk session, or if that kiosk
// session has been removed from policy since the session started, in which
// case we won't report its status).
std::unique_ptr<policy::DeviceLocalAccount> GetCurrentKioskDeviceLocalAccount(
    chromeos::CrosSettings* settings) {
  if (!user_manager::UserManager::Get()->IsLoggedInAsKioskApp() &&
      !user_manager::UserManager::Get()->IsLoggedInAsArcKioskApp()) {
    return std::unique_ptr<policy::DeviceLocalAccount>();
  }
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  const std::vector<policy::DeviceLocalAccount> accounts =
      policy::GetDeviceLocalAccounts(settings);

  for (const auto& device_local_account : accounts) {
    if (AccountId::FromUserEmail(device_local_account.user_id) ==
        user->GetAccountId()) {
      return base::MakeUnique<policy::DeviceLocalAccount>(device_local_account);
    }
  }
  LOG(WARNING) << "Kiosk app not found in list of device-local accounts";
  return std::unique_ptr<policy::DeviceLocalAccount>();
}

base::Version GetPlatformVersion() {
  int32_t major_version;
  int32_t minor_version;
  int32_t bugfix_version;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  return base::Version(base::StringPrintf("%d.%d.%d", major_version,
                                          minor_version, bugfix_version));
}

// Helper routine to convert from Shill-provided signal strength (percent)
// to dBm units expected by server.
int ConvertWifiSignalStrength(int signal_strength) {
  // Shill attempts to convert WiFi signal strength from its internal dBm to a
  // percentage range (from 0-100) by adding 120 to the raw dBm value,
  // and then clamping the result to the range 0-100 (see
  // shill::WiFiService::SignalToStrength()).
  //
  // To convert back to dBm, we subtract 120 from the percentage value to yield
  // a clamped dBm value in the range of -119 to -20dBm.
  //
  // TODO(atwilson): Tunnel the raw dBm signal strength from Shill instead of
  // doing the conversion here so we can report non-clamped values
  // (crbug.com/463334).
  DCHECK_GT(signal_strength, 0);
  DCHECK_LE(signal_strength, 100);
  return signal_strength - 120;
}

bool IsKioskApp() {
  auto user_type = chromeos::LoginState::Get()->GetLoggedInUserType();
  return user_type == chromeos::LoginState::LOGGED_IN_USER_KIOSK_APP ||
         user_type == chromeos::LoginState::LOGGED_IN_USER_ARC_KIOSK_APP;
}

}  // namespace

namespace policy {

// Helper class for state tracking of async status queries. Creates device and
// session status blobs in the constructor and sends them to the the status
// response callback in the destructor.
//
// Some methods like |SampleVolumeInfo| queue async queries to collect data. The
// response callback of these queries, e.g. |OnVolumeInfoReceived|, holds a
// reference to the instance of this class, so that the destructor will not be
// invoked and the status response callback will not be fired until the original
// owner of the instance releases its reference and all async queries finish.
//
// Therefore, if you create an instance of this class, make sure to release your
// reference after quering all async queries (if any), e.g. by using a local
// |scoped_refptr<GetStatusState>| and letting it go out of scope.
class GetStatusState : public base::RefCountedThreadSafe<GetStatusState> {
 public:
  explicit GetStatusState(
      const scoped_refptr<base::SequencedTaskRunner> task_runner,
      const policy::DeviceStatusCollector::StatusCallback& response)
      : task_runner_(task_runner), response_(response) {}

  inline em::DeviceStatusReportRequest* device_status() {
    return device_status_.get();
  }

  inline em::SessionStatusReportRequest* session_status() {
    return session_status_.get();
  }

  inline void ResetDeviceStatus() { device_status_.reset(); }

  inline void ResetSessionStatus() { session_status_.reset(); }

  // Queues an async callback to query disk volume information.
  void SampleVolumeInfo(const policy::DeviceStatusCollector::VolumeInfoFetcher&
                            volume_info_fetcher) {
    // Create list of mounted disk volumes to query status.
    std::vector<storage::MountPoints::MountPointInfo> external_mount_points;
    storage::ExternalMountPoints::GetSystemInstance()->AddMountPointInfosTo(
        &external_mount_points);

    std::vector<std::string> mount_points;
    for (const auto& info : external_mount_points)
      mount_points.push_back(info.path.value());

    for (const auto& mount_info :
         chromeos::disks::DiskMountManager::GetInstance()->mount_points()) {
      // Extract a list of mount points to populate.
      mount_points.push_back(mount_info.first);
    }

    // Call out to the blocking pool to sample disk volume info.
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::Bind(volume_info_fetcher, mount_points),
        base::Bind(&GetStatusState::OnVolumeInfoReceived, this));
  }

  // Queues an async callback to query CPU temperature information.
  void SampleCPUTempInfo(
      const policy::DeviceStatusCollector::CPUTempFetcher& cpu_temp_fetcher) {
    // Call out to the blocking pool to sample CPU temp.
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        cpu_temp_fetcher,
        base::Bind(&GetStatusState::OnCPUTempInfoReceived, this));
  }

  bool FetchAndroidStatus(
      const policy::DeviceStatusCollector::AndroidStatusFetcher&
          android_status_fetcher) {
    return android_status_fetcher.Run(
        base::Bind(&GetStatusState::OnAndroidInfoReceived, this));
  }

 private:
  friend class RefCountedThreadSafe<GetStatusState>;

  // Posts the response on the UI thread. As long as there is an outstanding
  // async query, the query holds a reference to us, so the destructor is
  // not called.
  ~GetStatusState() {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(response_, base::Passed(&device_status_),
                                  base::Passed(&session_status_)));
  }

  void OnVolumeInfoReceived(const std::vector<em::VolumeInfo>& volume_info) {
    device_status_->clear_volume_info();
    for (const em::VolumeInfo& info : volume_info)
      *device_status_->add_volume_info() = info;
  }

  void OnCPUTempInfoReceived(
      const std::vector<em::CPUTempInfo>& cpu_temp_info) {
    if (cpu_temp_info.empty())
      DLOG(WARNING) << "Unable to read CPU temp information.";

    device_status_->clear_cpu_temp_info();
    for (const em::CPUTempInfo& info : cpu_temp_info)
      *device_status_->add_cpu_temp_info() = info;
  }

  void OnAndroidInfoReceived(const std::string& status,
                             const std::string& droid_guard_info) {
    em::AndroidStatus* const android_status =
        session_status_->mutable_android_status();
    android_status->set_status_payload(status);
    android_status->set_droid_guard_info(droid_guard_info);
  }

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  policy::DeviceStatusCollector::StatusCallback response_;
  std::unique_ptr<em::DeviceStatusReportRequest> device_status_ =
      base::MakeUnique<em::DeviceStatusReportRequest>();
  std::unique_ptr<em::SessionStatusReportRequest> session_status_ =
      base::MakeUnique<em::SessionStatusReportRequest>();
};

DeviceStatusCollector::DeviceStatusCollector(
    PrefService* local_state,
    chromeos::system::StatisticsProvider* provider,
    const VolumeInfoFetcher& volume_info_fetcher,
    const CPUStatisticsFetcher& cpu_statistics_fetcher,
    const CPUTempFetcher& cpu_temp_fetcher,
    const AndroidStatusFetcher& android_status_fetcher)
    : max_stored_past_activity_days_(kMaxStoredPastActivityDays),
      max_stored_future_activity_days_(kMaxStoredFutureActivityDays),
      local_state_(local_state),
      last_idle_check_(Time()),
      volume_info_fetcher_(volume_info_fetcher),
      cpu_statistics_fetcher_(cpu_statistics_fetcher),
      cpu_temp_fetcher_(cpu_temp_fetcher),
      android_status_fetcher_(android_status_fetcher),
      statistics_provider_(provider),
      cros_settings_(chromeos::CrosSettings::Get()),
      task_runner_(nullptr),
      weak_factory_(this) {
  // Get the task runner of the current thread, so we can queue status responses
  // on this thread.
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  task_runner_ = base::SequencedTaskRunnerHandle::Get();

  if (volume_info_fetcher_.is_null())
    volume_info_fetcher_ = base::Bind(&GetVolumeInfo);

  if (cpu_statistics_fetcher_.is_null())
    cpu_statistics_fetcher_ = base::Bind(&ReadCPUStatistics);

  if (cpu_temp_fetcher_.is_null())
    cpu_temp_fetcher_ = base::Bind(&ReadCPUTempInfo);

  if (android_status_fetcher_.is_null())
    android_status_fetcher_ = base::Bind(&ReadAndroidStatus);

  idle_poll_timer_.Start(FROM_HERE,
                         TimeDelta::FromSeconds(kIdlePollIntervalSeconds),
                         this, &DeviceStatusCollector::CheckIdleState);
  resource_usage_sampling_timer_.Start(
      FROM_HERE, TimeDelta::FromSeconds(kResourceUsageSampleIntervalSeconds),
      this, &DeviceStatusCollector::SampleResourceUsage);

  // Watch for changes to the individual policies that control what the status
  // reports contain.
  base::Closure callback =
      base::Bind(&DeviceStatusCollector::UpdateReportingSettings,
                 base::Unretained(this));
  version_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceVersionInfo, callback);
  activity_times_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceActivityTimes, callback);
  boot_mode_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceBootMode, callback);
  network_interfaces_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceNetworkInterfaces, callback);
  users_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceUsers, callback);
  hardware_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceHardwareStatus, callback);
  session_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceSessionStatus, callback);
  os_update_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportOsUpdateStatus, callback);
  running_kiosk_app_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportRunningKioskApp, callback);

  // Fetch the current values of the policies.
  UpdateReportingSettings();

  // Get the OS, firmware, and TPM version info.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&chromeos::version_loader::GetVersion,
                 chromeos::version_loader::VERSION_FULL),
      base::Bind(&DeviceStatusCollector::OnOSVersion,
                 weak_factory_.GetWeakPtr()));
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&chromeos::version_loader::GetFirmware),
      base::Bind(&DeviceStatusCollector::OnOSFirmware,
                 weak_factory_.GetWeakPtr()));
  chromeos::version_loader::GetTpmVersion(
      base::BindOnce(&DeviceStatusCollector::OnTpmVersion,
                     weak_factory_.GetWeakPtr()));
}

DeviceStatusCollector::~DeviceStatusCollector() {
}

// static
void DeviceStatusCollector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kDeviceActivityTimes,
                                   base::MakeUnique<base::DictionaryValue>());
}

// static
void DeviceStatusCollector::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kReportArcStatusEnabled, false);
}

void DeviceStatusCollector::CheckIdleState() {
  CalculateIdleState(kIdleStateThresholdSeconds,
      base::Bind(&DeviceStatusCollector::IdleStateCallback,
                 base::Unretained(this)));
}

void DeviceStatusCollector::UpdateReportingSettings() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  if (chromeos::CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
      base::Bind(&DeviceStatusCollector::UpdateReportingSettings,
                 weak_factory_.GetWeakPtr()))) {
    return;
  }

  // All reporting settings default to 'enabled'.
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceVersionInfo, &report_version_info_)) {
    report_version_info_ = true;
  }
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceActivityTimes, &report_activity_times_)) {
    report_activity_times_ = true;
  }
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceBootMode, &report_boot_mode_)) {
    report_boot_mode_ = true;
  }
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceNetworkInterfaces,
          &report_network_interfaces_)) {
    report_network_interfaces_ = true;
  }
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceUsers, &report_users_)) {
    report_users_ = true;
  }

  const bool already_reporting_hardware_status = report_hardware_status_;
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceHardwareStatus, &report_hardware_status_)) {
    report_hardware_status_ = true;
  }

  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceSessionStatus,
                                  &report_kiosk_session_status_)) {
    report_kiosk_session_status_ = true;
  }

  if (!report_hardware_status_) {
    ClearCachedResourceUsage();
  } else if (!already_reporting_hardware_status) {
    // Turning on hardware status reporting - fetch an initial sample
    // immediately instead of waiting for the sampling timer to fire.
    SampleResourceUsage();
  }

  // Os update status and running kiosk app reporting are disabled by default.
  if (!cros_settings_->GetBoolean(chromeos::kReportOsUpdateStatus,
                                  &report_os_update_status_)) {
    report_os_update_status_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportRunningKioskApp,
                                  &report_running_kiosk_app_)) {
    report_running_kiosk_app_ = false;
  }
}

Time DeviceStatusCollector::GetCurrentTime() {
  return Time::Now();
}

// Remove all out-of-range activity times from the local store.
void DeviceStatusCollector::PruneStoredActivityPeriods(Time base_time) {
  Time min_time =
      base_time - TimeDelta::FromDays(max_stored_past_activity_days_);
  Time max_time =
      base_time + TimeDelta::FromDays(max_stored_future_activity_days_);
  TrimStoredActivityPeriods(TimestampToDayKey(min_time), 0,
                            TimestampToDayKey(max_time));
}

void DeviceStatusCollector::TrimStoredActivityPeriods(int64_t min_day_key,
                                                      int min_day_trim_duration,
                                                      int64_t max_day_key) {
  const base::DictionaryValue* activity_times =
      local_state_->GetDictionary(prefs::kDeviceActivityTimes);

  std::unique_ptr<base::DictionaryValue> copy(activity_times->DeepCopy());
  for (base::DictionaryValue::Iterator it(*activity_times); !it.IsAtEnd();
       it.Advance()) {
    int64_t timestamp;
    if (base::StringToInt64(it.key(), &timestamp)) {
      // Remove data that is too old, or too far in the future.
      if (timestamp >= min_day_key && timestamp < max_day_key) {
        if (timestamp == min_day_key) {
          int new_activity_duration = 0;
          if (it.value().GetAsInteger(&new_activity_duration)) {
            new_activity_duration =
                std::max(new_activity_duration - min_day_trim_duration, 0);
          }
          copy->SetInteger(it.key(), new_activity_duration);
        }
        continue;
      }
    }
    // The entry is out of range or couldn't be parsed. Remove it.
    copy->Remove(it.key(), NULL);
  }
  local_state_->Set(prefs::kDeviceActivityTimes, *copy);
}

void DeviceStatusCollector::AddActivePeriod(Time start, Time end) {
  DCHECK(start < end);

  // Maintain the list of active periods in a local_state pref.
  DictionaryPrefUpdate update(local_state_, prefs::kDeviceActivityTimes);
  base::DictionaryValue* activity_times = update.Get();

  // Assign the period to day buckets in local time.
  Time midnight = start.LocalMidnight();
  while (midnight < end) {
    midnight += TimeDelta::FromDays(1);
    int64_t activity = (std::min(end, midnight) - start).InMilliseconds();
    std::string day_key = base::Int64ToString(TimestampToDayKey(start));
    int previous_activity = 0;
    activity_times->GetInteger(day_key, &previous_activity);
    activity_times->SetInteger(day_key, previous_activity + activity);
    start = midnight;
  }
}

void DeviceStatusCollector::ClearCachedResourceUsage() {
  resource_usage_.clear();
  last_cpu_active_ = 0;
  last_cpu_idle_ = 0;
}

void DeviceStatusCollector::IdleStateCallback(ui::IdleState state) {
  // Do nothing if device activity reporting is disabled.
  if (!report_activity_times_)
    return;

  Time now = GetCurrentTime();

  // For kiosk apps we report total uptime instead of active time.
  if (state == ui::IDLE_STATE_ACTIVE || IsKioskApp()) {
    // If it's been too long since the last report, or if the activity is
    // negative (which can happen when the clock changes), assume a single
    // interval of activity.
    int active_seconds = (now - last_idle_check_).InSeconds();
    if (active_seconds < 0 ||
        active_seconds >= static_cast<int>((2 * kIdlePollIntervalSeconds))) {
      AddActivePeriod(now - TimeDelta::FromSeconds(kIdlePollIntervalSeconds),
                      now);
    } else {
      AddActivePeriod(last_idle_check_, now);
    }

    PruneStoredActivityPeriods(now);
  }
  last_idle_check_ = now;
}

std::unique_ptr<DeviceLocalAccount>
DeviceStatusCollector::GetAutoLaunchedKioskSessionInfo() {
  std::unique_ptr<DeviceLocalAccount> account =
      GetCurrentKioskDeviceLocalAccount(cros_settings_);
  if (account) {
    chromeos::KioskAppManager::App current_app;
    bool regular_app_auto_launched_with_zero_delay =
        chromeos::KioskAppManager::Get()->GetApp(account->kiosk_app_id,
                                                 &current_app) &&
        current_app.was_auto_launched_with_zero_delay;
    bool arc_app_auto_launched_with_zero_delay =
        chromeos::ArcKioskAppManager::Get()
            ->current_app_was_auto_launched_with_zero_delay();
    if (regular_app_auto_launched_with_zero_delay ||
        arc_app_auto_launched_with_zero_delay) {
      return account;
    }
  }
  // No auto-launched kiosk session active.
  return std::unique_ptr<DeviceLocalAccount>();
}

void DeviceStatusCollector::SampleResourceUsage() {
  // Results must be written in the creation thread since that's where they
  // are read from in the Get*StatusAsync methods.
  DCHECK(thread_checker_.CalledOnValidThread());

  // If hardware reporting has been disabled, do nothing here.
  if (!report_hardware_status_)
    return;

  // Call out to the blocking pool to sample CPU stats.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      cpu_statistics_fetcher_,
      base::Bind(&DeviceStatusCollector::ReceiveCPUStatistics,
                 weak_factory_.GetWeakPtr()));
}

void DeviceStatusCollector::ReceiveCPUStatistics(const std::string& stats) {
  int cpu_usage_percent = 0;
  if (stats.empty()) {
    DLOG(WARNING) << "Unable to read CPU statistics";
  } else {
    // Parse the data from /proc/stat, whose format is defined at
    // https://www.kernel.org/doc/Documentation/filesystems/proc.txt.
    //
    // The CPU usage values in /proc/stat are measured in the imprecise unit
    // "jiffies", but we just care about the relative magnitude of "active" vs
    // "idle" so the exact value of a jiffy is irrelevant.
    //
    // An example value for this line:
    //
    // cpu 123 456 789 012 345 678
    //
    // We only care about the first four numbers: user_time, nice_time,
    // sys_time, and idle_time.
    uint64_t user = 0, nice = 0, system = 0, idle = 0;
    int vals = sscanf(stats.c_str(),
                      "cpu %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, &user,
                      &nice, &system, &idle);
    DCHECK_EQ(4, vals);

    // The values returned from /proc/stat are cumulative totals, so calculate
    // the difference between the last sample and this one.
    uint64_t active = user + nice + system;
    uint64_t total = active + idle;
    uint64_t last_total = last_cpu_active_ + last_cpu_idle_;
    DCHECK_GE(active, last_cpu_active_);
    DCHECK_GE(idle, last_cpu_idle_);
    DCHECK_GE(total, last_total);

    if ((total - last_total) > 0) {
      cpu_usage_percent =
          (100 * (active - last_cpu_active_)) / (total - last_total);
    }
    last_cpu_active_ = active;
    last_cpu_idle_ = idle;
  }

  DCHECK_LE(cpu_usage_percent, 100);
  ResourceUsage usage = {cpu_usage_percent,
                         base::SysInfo::AmountOfAvailablePhysicalMemory()};

  resource_usage_.push_back(usage);

  // If our cache of samples is full, throw out old samples to make room for new
  // sample.
  if (resource_usage_.size() > kMaxResourceUsageSamples)
    resource_usage_.pop_front();
}

bool DeviceStatusCollector::GetActivityTimes(
    em::DeviceStatusReportRequest* status) {
  DictionaryPrefUpdate update(local_state_, prefs::kDeviceActivityTimes);
  base::DictionaryValue* activity_times = update.Get();

  bool anything_reported = false;
  for (base::DictionaryValue::Iterator it(*activity_times); !it.IsAtEnd();
       it.Advance()) {
    int64_t start_timestamp;
    int activity_milliseconds;
    if (base::StringToInt64(it.key(), &start_timestamp) &&
        it.value().GetAsInteger(&activity_milliseconds)) {
      // This is correct even when there are leap seconds, because when a leap
      // second occurs, two consecutive seconds have the same timestamp.
      int64_t end_timestamp = start_timestamp + Time::kMillisecondsPerDay;

      em::ActiveTimePeriod* active_period = status->add_active_period();
      em::TimePeriod* period = active_period->mutable_time_period();
      period->set_start_timestamp(start_timestamp);
      period->set_end_timestamp(end_timestamp);
      active_period->set_active_duration(activity_milliseconds);
      if (start_timestamp >= last_reported_day_) {
        last_reported_day_ = start_timestamp;
        duration_for_last_reported_day_ = activity_milliseconds;
      }
      anything_reported = true;
    } else {
      NOTREACHED();
    }
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetVersionInfo(
    em::DeviceStatusReportRequest* status) {
  status->set_browser_version(version_info::GetVersionNumber());
  status->set_os_version(os_version_);
  status->set_firmware_version(firmware_version_);

  em::TpmVersionInfo* const tpm_version_info =
      status->mutable_tpm_version_info();
  tpm_version_info->set_family(tpm_version_info_.family);
  tpm_version_info->set_spec_level(tpm_version_info_.spec_level);
  tpm_version_info->set_manufacturer(tpm_version_info_.manufacturer);
  tpm_version_info->set_tpm_model(tpm_version_info_.tpm_model);
  tpm_version_info->set_firmware_version(tpm_version_info_.firmware_version);
  tpm_version_info->set_vendor_specific(tpm_version_info_.vendor_specific);

  return true;
}

bool DeviceStatusCollector::GetBootMode(em::DeviceStatusReportRequest* status) {
  std::string dev_switch_mode;
  bool anything_reported = false;
  if (statistics_provider_->GetMachineStatistic(
          chromeos::system::kDevSwitchBootKey, &dev_switch_mode)) {
    if (dev_switch_mode == chromeos::system::kDevSwitchBootValueDev)
      status->set_boot_mode("Dev");
    else if (dev_switch_mode == chromeos::system::kDevSwitchBootValueVerified)
      status->set_boot_mode("Verified");
    anything_reported = true;
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetNetworkInterfaces(
    em::DeviceStatusReportRequest* status) {
  // Maps shill device type strings to proto enum constants.
  static const struct {
    const char* type_string;
    em::NetworkInterface::NetworkDeviceType type_constant;
  } kDeviceTypeMap[] = {
    { shill::kTypeEthernet,  em::NetworkInterface::TYPE_ETHERNET,  },
    { shill::kTypeWifi,      em::NetworkInterface::TYPE_WIFI,      },
    { shill::kTypeWimax,     em::NetworkInterface::TYPE_WIMAX,     },
    { shill::kTypeBluetooth, em::NetworkInterface::TYPE_BLUETOOTH, },
    { shill::kTypeCellular,  em::NetworkInterface::TYPE_CELLULAR,  },
  };

  // Maps shill device connection status to proto enum constants.
  static const struct {
    const char* state_string;
    em::NetworkState::ConnectionState state_constant;
  } kConnectionStateMap[] = {
    { shill::kStateIdle,              em::NetworkState::IDLE },
    { shill::kStateCarrier,           em::NetworkState::CARRIER },
    { shill::kStateAssociation,       em::NetworkState::ASSOCIATION },
    { shill::kStateConfiguration,     em::NetworkState::CONFIGURATION },
    { shill::kStateReady,             em::NetworkState::READY },
    { shill::kStatePortal,            em::NetworkState::PORTAL },
    { shill::kStateOffline,           em::NetworkState::OFFLINE },
    { shill::kStateOnline,            em::NetworkState::ONLINE },
    { shill::kStateDisconnect,        em::NetworkState::DISCONNECT },
    { shill::kStateFailure,           em::NetworkState::FAILURE },
    { shill::kStateActivationFailure,
        em::NetworkState::ACTIVATION_FAILURE },
  };

  chromeos::NetworkStateHandler::DeviceStateList device_list;
  chromeos::NetworkStateHandler* network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  network_state_handler->GetDeviceList(&device_list);

  bool anything_reported = false;
  chromeos::NetworkStateHandler::DeviceStateList::const_iterator device;
  for (device = device_list.begin(); device != device_list.end(); ++device) {
    // Determine the type enum constant for |device|.
    size_t type_idx = 0;
    for (; type_idx < arraysize(kDeviceTypeMap); ++type_idx) {
      if ((*device)->type() == kDeviceTypeMap[type_idx].type_string)
        break;
    }

    // If the type isn't in |kDeviceTypeMap|, the interface is not relevant for
    // reporting. This filters out VPN devices.
    if (type_idx >= arraysize(kDeviceTypeMap))
      continue;

    em::NetworkInterface* interface = status->add_network_interface();
    interface->set_type(kDeviceTypeMap[type_idx].type_constant);
    if (!(*device)->mac_address().empty())
      interface->set_mac_address((*device)->mac_address());
    if (!(*device)->meid().empty())
      interface->set_meid((*device)->meid());
    if (!(*device)->imei().empty())
      interface->set_imei((*device)->imei());
    if (!(*device)->path().empty())
      interface->set_device_path((*device)->path());
    anything_reported = true;
  }

  // Don't write any network state if we aren't in a kiosk or public session.
  if (!GetAutoLaunchedKioskSessionInfo() &&
      !user_manager::UserManager::Get()->IsLoggedInAsPublicAccount())
    return anything_reported;

  // Walk the various networks and store their state in the status report.
  chromeos::NetworkStateHandler::NetworkStateList state_list;
  network_state_handler->GetNetworkListByType(
      chromeos::NetworkTypePattern::Default(),
      true,   // configured_only
      false,  // visible_only
      0,      // no limit to number of results
      &state_list);

  for (const chromeos::NetworkState* state : state_list) {
    // Determine the connection state and signal strength for |state|.
    em::NetworkState::ConnectionState connection_state_enum =
        em::NetworkState::UNKNOWN;
    const std::string connection_state_string(state->connection_state());
    for (size_t i = 0; i < arraysize(kConnectionStateMap); ++i) {
      if (connection_state_string == kConnectionStateMap[i].state_string) {
        connection_state_enum = kConnectionStateMap[i].state_constant;
        break;
      }
    }

    // Copy fields from NetworkState into the status report.
    em::NetworkState* proto_state = status->add_network_state();
    proto_state->set_connection_state(connection_state_enum);
    anything_reported = true;

    // Report signal strength for wifi connections.
    if (state->type() == shill::kTypeWifi) {
      // If shill has provided a signal strength, convert it to dBm and store it
      // in the status report. A signal_strength() of 0 connotes "no signal"
      // rather than "really weak signal", so we only report signal strength if
      // it is non-zero.
      if (state->signal_strength()) {
        proto_state->set_signal_strength(
            ConvertWifiSignalStrength(state->signal_strength()));
      }
    }

    if (!state->device_path().empty())
      proto_state->set_device_path(state->device_path());

    std::string ip_address = state->GetIpAddress();
    if (!ip_address.empty())
      proto_state->set_ip_address(ip_address);

    std::string gateway = state->GetGateway();
    if (!gateway.empty())
      proto_state->set_gateway(gateway);
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetUsers(em::DeviceStatusReportRequest* status) {
  const user_manager::UserList& users =
      chromeos::ChromeUserManager::Get()->GetUsers();

  bool anything_reported = false;
  for (auto* user : users) {
    // Only users with gaia accounts (regular) are reported.
    if (!user->HasGaiaAccount())
      continue;

    em::DeviceUser* device_user = status->add_user();
    if (chromeos::ChromeUserManager::Get()->ShouldReportUser(
            user->GetAccountId().GetUserEmail())) {
      device_user->set_type(em::DeviceUser::USER_TYPE_MANAGED);
      device_user->set_email(user->GetAccountId().GetUserEmail());
    } else {
      device_user->set_type(em::DeviceUser::USER_TYPE_UNMANAGED);
      // Do not report the email address of unmanaged users.
    }
    anything_reported = true;
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetHardwareStatus(
    em::DeviceStatusReportRequest* status,
    scoped_refptr<GetStatusState> state) {
  // Sample disk volume info in a background thread.
  state->SampleVolumeInfo(volume_info_fetcher_);

  // Sample CPU temperature in a background thread.
  state->SampleCPUTempInfo(cpu_temp_fetcher_);

  // Add CPU utilization and free RAM. Note that these stats are sampled in
  // regular intervals. Unlike CPU temp and volume info these are not one-time
  // sampled values, hence the difference in logic.
  status->set_system_ram_total(base::SysInfo::AmountOfPhysicalMemory());
  status->clear_system_ram_free();
  status->clear_cpu_utilization_pct();
  for (const ResourceUsage& usage : resource_usage_) {
    status->add_cpu_utilization_pct(usage.cpu_usage_percent);
    status->add_system_ram_free(usage.bytes_of_ram_free);
  }

  // Get the current device sound volume level.
  chromeos::CrasAudioHandler* audio_handler = chromeos::CrasAudioHandler::Get();
  status->set_sound_volume(audio_handler->GetOutputVolumePercent());

  return true;
}

bool DeviceStatusCollector::GetOsUpdateStatus(
    em::DeviceStatusReportRequest* status) {
  const base::Version platform_version(GetPlatformVersion());
  if (!platform_version.IsValid())
    return false;

  const std::string required_platform_version_string =
      chromeos::KioskAppManager::Get()
          ->GetAutoLaunchAppRequiredPlatformVersion();
  if (required_platform_version_string.empty())
    return false;

  const base::Version required_platfrom_version(
      required_platform_version_string);

  em::OsUpdateStatus* os_update_status = status->mutable_os_update_status();
  os_update_status->set_new_required_platform_version(
      required_platfrom_version.GetString());

  if (platform_version == required_platfrom_version) {
    os_update_status->set_update_status(em::OsUpdateStatus::OS_UP_TO_DATE);
    return true;
  }

  const chromeos::UpdateEngineClient::Status update_engine_status =
      chromeos::DBusThreadManager::Get()
          ->GetUpdateEngineClient()
          ->GetLastStatus();
  if (update_engine_status.status ==
          chromeos::UpdateEngineClient::UPDATE_STATUS_DOWNLOADING ||
      update_engine_status.status ==
          chromeos::UpdateEngineClient::UPDATE_STATUS_VERIFYING ||
      update_engine_status.status ==
          chromeos::UpdateEngineClient::UPDATE_STATUS_FINALIZING) {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_IN_PROGRESS);
    os_update_status->set_new_platform_version(
        update_engine_status.new_version);
  } else if (update_engine_status.status ==
             chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_UPDATE_NEED_REBOOT);
    // Note the new_version could be a dummy "0.0.0.0" for some edge cases,
    // e.g. update engine is somehow restarted without a reboot.
    os_update_status->set_new_platform_version(
        update_engine_status.new_version);
  } else {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_NOT_STARTED);
  }

  return true;
}

bool DeviceStatusCollector::GetRunningKioskApp(
    em::DeviceStatusReportRequest* status) {
  // Must be on creation thread since some stats are written to in that thread
  // and accessing them from another thread would lead to race conditions.
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<const DeviceLocalAccount> account =
      GetAutoLaunchedKioskSessionInfo();
  // Only generate running kiosk app reports if we are in an auto-launched kiosk
  // session.
  if (!account)
    return false;

  em::AppStatus* running_kiosk_app = status->mutable_running_kiosk_app();
  running_kiosk_app->set_app_id(account->kiosk_app_id);

  const std::string app_version = GetAppVersion(account->kiosk_app_id);
  if (app_version.empty()) {
    DLOG(ERROR) << "Unable to get version for extension: "
                << account->kiosk_app_id;
  } else {
    running_kiosk_app->set_extension_version(app_version);
  }

  chromeos::KioskAppManager::App app_info;
  if (chromeos::KioskAppManager::Get()->GetApp(account->kiosk_app_id,
                                               &app_info)) {
    running_kiosk_app->set_required_platform_version(
        app_info.required_platform_version);
  }
  return true;
}

void DeviceStatusCollector::GetDeviceAndSessionStatusAsync(
    const StatusCallback& response) {
  // Must be on creation thread since some stats are written to in that thread
  // and accessing them from another thread would lead to race conditions.
  DCHECK(thread_checker_.CalledOnValidThread());

  // Some of the data we're collecting is gathered in background threads.
  // This object keeps track of the state of each async request.
  scoped_refptr<GetStatusState> state(
      new GetStatusState(task_runner_, response));

  // Gather device status (might queue some async queries)
  GetDeviceStatus(state);

  // Gather session status (might queue some async queries)
  GetSessionStatus(state);

  // If there are no outstanding async queries, e.g. from GetHardwareStatus(),
  // the destructor of |state| calls |response|. If there are async queries, the
  // queries hold references to |state|, so that |state| is only destroyed when
  // the last async query has finished.
}

void DeviceStatusCollector::GetDeviceStatus(
    scoped_refptr<GetStatusState> state) {
  em::DeviceStatusReportRequest* status = state->device_status();
  bool anything_reported = false;

  if (report_activity_times_)
    anything_reported |= GetActivityTimes(status);

  if (report_version_info_)
    anything_reported |= GetVersionInfo(status);

  if (report_boot_mode_)
    anything_reported |= GetBootMode(status);

  if (report_network_interfaces_)
    anything_reported |= GetNetworkInterfaces(status);

  if (report_users_)
    anything_reported |= GetUsers(status);

  if (report_hardware_status_)
    anything_reported |= GetHardwareStatus(status, state);

  if (report_os_update_status_)
    anything_reported |= GetOsUpdateStatus(status);

  if (report_running_kiosk_app_)
    anything_reported |= GetRunningKioskApp(status);

  // Wipe pointer if we didn't actually add any data.
  if (!anything_reported)
    state->ResetDeviceStatus();
}

std::string DeviceStatusCollector::GetDMTokenForProfile(Profile* profile) {
  CloudPolicyManager* user_cloud_policy_manager =
      UserPolicyManagerFactoryChromeOS::GetCloudPolicyManagerForProfile(
          profile);
  if (!user_cloud_policy_manager) {
    NOTREACHED();
    return std::string();
  }

  auto* cloud_policy_client = user_cloud_policy_manager->core()->client();
  std::string dm_token = cloud_policy_client->dm_token();

  return dm_token;
}

bool DeviceStatusCollector::GetSessionStatusForUser(
    scoped_refptr<GetStatusState> state,
    em::SessionStatusReportRequest* status,
    const user_manager::User* user) {
  Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return false;

  bool anything_reported_user = false;
  bool report_android_status =
      profile->GetPrefs()->GetBoolean(prefs::kReportArcStatusEnabled);

  if (report_android_status)
    anything_reported_user |= GetAndroidStatus(status, state);
  if (anything_reported_user && !user->IsDeviceLocalAccount())
    status->set_user_dm_token(GetDMTokenForProfile(profile));
  return anything_reported_user;
}

void DeviceStatusCollector::GetSessionStatus(
    scoped_refptr<GetStatusState> state) {
  em::SessionStatusReportRequest* status = state->session_status();
  bool anything_reported = false;

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* const primary_user = user_manager->GetPrimaryUser();

  if (report_kiosk_session_status_)
    anything_reported |= GetKioskSessionStatus(status);

  // Only report user data for affiliated users. Note that device-local
  // accounts are also affiliated.
  // Currently we only report for the primary user.
  if (primary_user && primary_user->IsAffiliated()) {
    anything_reported |= GetSessionStatusForUser(state, status, primary_user);
  }

  // Wipe pointer if we didn't actually add any data.
  if (!anything_reported)
    state->ResetSessionStatus();
}

bool DeviceStatusCollector::GetKioskSessionStatus(
    em::SessionStatusReportRequest* status) {
  std::unique_ptr<const DeviceLocalAccount> account =
      GetAutoLaunchedKioskSessionInfo();
  if (!account)
    return false;

  // Get the account ID associated with this user.
  status->set_device_local_account_id(account->account_id);
  em::AppStatus* app_status = status->add_installed_apps();
  app_status->set_app_id(account->kiosk_app_id);

  // Look up the app and get the version.
  const std::string app_version = GetAppVersion(account->kiosk_app_id);
  if (app_version.empty()) {
    DLOG(ERROR) << "Unable to get version for extension: "
                << account->kiosk_app_id;
  } else {
    app_status->set_extension_version(app_version);
  }

  return true;
}

bool DeviceStatusCollector::GetAndroidStatus(
    em::SessionStatusReportRequest* status,
    const scoped_refptr<GetStatusState>& state) {
  return state->FetchAndroidStatus(android_status_fetcher_);
}

std::string DeviceStatusCollector::GetAppVersion(
    const std::string& kiosk_app_id) {
  Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(
          user_manager::UserManager::Get()->GetActiveUser());
  const extensions::ExtensionRegistry* const registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* const extension = registry->GetExtensionById(
      kiosk_app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (!extension)
    return std::string();
  return extension->VersionString();
}

void DeviceStatusCollector::OnSubmittedSuccessfully() {
  TrimStoredActivityPeriods(last_reported_day_, duration_for_last_reported_day_,
                            std::numeric_limits<int64_t>::max());
}

void DeviceStatusCollector::OnOSVersion(const std::string& version) {
  os_version_ = version;
}

void DeviceStatusCollector::OnOSFirmware(const std::string& version) {
  firmware_version_ = version;
}

void DeviceStatusCollector::OnTpmVersion(
    const chromeos::CryptohomeClient::TpmVersionInfo& tpm_version_info) {
  tpm_version_info_ = tpm_version_info;
}

}  // namespace policy
