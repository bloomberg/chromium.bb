// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/statistics_provider.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chromeos/app_mode/kiosk_oem_manifest_parser.h"
#include "chromeos/chromeos_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/system/name_value_pairs_parser.h"

namespace chromeos {
namespace system {

namespace {

// Path to the tool used to get system info, and delimiters for the output
// format of the tool.
const char* kCrosSystemTool[] = { "/usr/bin/crossystem" };
const char kCrosSystemEq[] = "=";
const char kCrosSystemDelim[] = "\n";
const char kCrosSystemCommentDelim[] = "#";
const char kCrosSystemUnknownValue[] = "(error)";

const char kHardwareClassCrosSystemKey[] = "hwid";
const char kUnknownHardwareClass[] = "unknown";
const char kSerialNumber[] = "sn";

// File to get machine hardware info from, and key/value delimiters of
// the file. machine-info is generated only for OOBE and enterprise enrollment
// and may not be present. See login-manager/init/machine-info.conf.
const char kMachineHardwareInfoFile[] = "/tmp/machine-info";
const char kMachineHardwareInfoEq[] = "=";
const char kMachineHardwareInfoDelim[] = " \n";

// File to get ECHO coupon info from, and key/value delimiters of
// the file.
const char kEchoCouponFile[] = "/var/cache/echo/vpd_echo.txt";
const char kEchoCouponEq[] = "=";
const char kEchoCouponDelim[] = "\n";

// File to get VPD info from, and key/value delimiters of the file.
const char kVpdFile[] = "/var/log/vpd_2.0.txt";
const char kVpdEq[] = "=";
const char kVpdDelim[] = "\n";

// Timeout that we should wait for statistics to get loaded
const int kTimeoutSecs = 3;

// The location of OEM manifest file used to trigger OOBE flow for kiosk mode.
const CommandLine::CharType kOemManifestFilePath[] =
    FILE_PATH_LITERAL("/usr/share/oem/oobe/manifest.json");

}  // namespace

// Key values for GetMachineStatistic()/GetMachineFlag() calls.
const char kDevSwitchBootMode[] = "devsw_boot";
const char kCustomizationIdKey[] = "customization_id";
const char kHardwareClassKey[] = "hardware_class";
const char kOffersCouponCodeKey[] = "ubind_attribute";
const char kOffersGroupCodeKey[] = "gbind_attribute";
const char kRlzBrandCodeKey[] = "rlz_brand_code";

// OEM specific statistics. Must be prefixed with "oem_".
const char kOemCanExitEnterpriseEnrollmentKey[] = "oem_can_exit_enrollment";
const char kOemDeviceRequisitionKey[] = "oem_device_requisition";
const char kOemIsEnterpriseManagedKey[] = "oem_enterprise_managed";
const char kOemKeyboardDrivenOobeKey[] = "oem_keyboard_driven_oobe";

bool HasOemPrefix(const std::string& name) {
  return name.substr(0, 4) == "oem_";
}

// The StatisticsProvider implementation used in production.
class StatisticsProviderImpl : public StatisticsProvider {
 public:
  // StatisticsProvider implementation:
  virtual void StartLoadingMachineStatistics(
      const scoped_refptr<base::TaskRunner>& file_task_runner,
      bool load_oem_manifest) OVERRIDE;
  virtual bool GetMachineStatistic(const std::string& name,
                                   std::string* result) OVERRIDE;
  virtual bool GetMachineFlag(const std::string& name, bool* result) OVERRIDE;
  virtual void Shutdown() OVERRIDE;

  static StatisticsProviderImpl* GetInstance();

 protected:
  typedef std::map<std::string, bool> MachineFlags;
  friend struct DefaultSingletonTraits<StatisticsProviderImpl>;

  StatisticsProviderImpl();
  virtual ~StatisticsProviderImpl();

  // Waits up to |kTimeoutSecs| for statistics to be loaded. Returns true if
  // they were loaded successfully.
  bool WaitForStatisticsLoaded();

  // Loads the machine statistics off of disk. Runs on the file thread.
  void LoadMachineStatistics(bool load_oem_manifest);

  // Loads the OEM statistics off of disk. Runs on the file thread.
  void LoadOemManifestFromFile(const base::FilePath& file);

  bool load_statistics_started_;
  NameValuePairsParser::NameValueMap machine_info_;
  MachineFlags machine_flags_;
  base::CancellationFlag cancellation_flag_;
  // |on_statistics_loaded_| protects |machine_info_| and |machine_flags_|.
  base::WaitableEvent on_statistics_loaded_;
  bool oem_manifest_loaded_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatisticsProviderImpl);
};

bool StatisticsProviderImpl::WaitForStatisticsLoaded() {
  CHECK(load_statistics_started_);
  if (on_statistics_loaded_.IsSignaled())
    return true;

  // Block if the statistics are not loaded yet. Normally this shouldn't
  // happen except during OOBE.
  base::Time start_time = base::Time::Now();
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  on_statistics_loaded_.TimedWait(base::TimeDelta::FromSeconds(kTimeoutSecs));

  base::TimeDelta dtime = base::Time::Now() - start_time;
  if (on_statistics_loaded_.IsSignaled()) {
    LOG(ERROR) << "Statistics loaded after waiting "
               << dtime.InMilliseconds() << "ms. ";
    return true;
  }

  LOG(ERROR) << "Statistics not loaded after waiting "
             << dtime.InMilliseconds() << "ms. ";
  return false;
}

bool StatisticsProviderImpl::GetMachineStatistic(const std::string& name,
                                                 std::string* result) {
  VLOG(1) << "Machine Statistic requested: " << name;
  if (!WaitForStatisticsLoaded()) {
    LOG(ERROR) << "GetMachineStatistic called before load started: " << name;
    return false;
  }

  NameValuePairsParser::NameValueMap::iterator iter = machine_info_.find(name);
  if (iter == machine_info_.end()) {
    if (base::SysInfo::IsRunningOnChromeOS() &&
        (oem_manifest_loaded_ || !HasOemPrefix(name))) {
      LOG(WARNING) << "Requested statistic not found: " << name;
    }
    return false;
  }
  *result = iter->second;
  return true;
}

bool StatisticsProviderImpl::GetMachineFlag(const std::string& name,
                                            bool* result) {
  VLOG(1) << "Machine Flag requested: " << name;
  if (!WaitForStatisticsLoaded()) {
    LOG(ERROR) << "GetMachineFlag called before load started: " << name;
    return false;
  }

  MachineFlags::const_iterator iter = machine_flags_.find(name);
  if (iter == machine_flags_.end()) {
    if (base::SysInfo::IsRunningOnChromeOS() &&
        (oem_manifest_loaded_ || !HasOemPrefix(name))) {
      LOG(WARNING) << "Requested machine flag not found: " << name;
    }
    return false;
  }
  *result = iter->second;
  return true;
}

void StatisticsProviderImpl::Shutdown() {
  cancellation_flag_.Set();  // Cancel any pending loads
}

StatisticsProviderImpl::StatisticsProviderImpl()
    : load_statistics_started_(false),
      on_statistics_loaded_(true  /* manual_reset */,
                            false /* initially_signaled */),
      oem_manifest_loaded_(false) {
}

StatisticsProviderImpl::~StatisticsProviderImpl() {
}

void StatisticsProviderImpl::StartLoadingMachineStatistics(
    const scoped_refptr<base::TaskRunner>& file_task_runner,
    bool load_oem_manifest) {
  CHECK(!load_statistics_started_);
  load_statistics_started_ = true;

  VLOG(1) << "Started loading statistics. Load OEM Manifest: "
          << load_oem_manifest;

  file_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&StatisticsProviderImpl::LoadMachineStatistics,
                 base::Unretained(this),
                 load_oem_manifest));
}

void StatisticsProviderImpl::LoadMachineStatistics(bool load_oem_manifest) {
  // Run from the file task runner. StatisticsProviderImpl is a Singleton<> and
  // will not be destroyed until after threads have been stopped, so this test
  // is always safe.
  if (cancellation_flag_.IsSet())
    return;

  NameValuePairsParser parser(&machine_info_);
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // Parse all of the key/value pairs from the crossystem tool.
    if (!parser.ParseNameValuePairsFromTool(arraysize(kCrosSystemTool),
                                            kCrosSystemTool,
                                            kCrosSystemEq,
                                            kCrosSystemDelim,
                                            kCrosSystemCommentDelim)) {
      LOG(ERROR) << "Errors parsing output from: " << kCrosSystemTool;
    }
  }

  parser.GetNameValuePairsFromFile(base::FilePath(kMachineHardwareInfoFile),
                                   kMachineHardwareInfoEq,
                                   kMachineHardwareInfoDelim);
  parser.GetNameValuePairsFromFile(base::FilePath(kEchoCouponFile),
                                   kEchoCouponEq,
                                   kEchoCouponDelim);
  parser.GetNameValuePairsFromFile(base::FilePath(kVpdFile),
                                   kVpdEq,
                                   kVpdDelim);

  // Ensure that the hardware class key is present with the expected
  // key name, and if it couldn't be retrieved, that the value is "unknown".
  std::string hardware_class = machine_info_[kHardwareClassCrosSystemKey];
  if (hardware_class.empty() || hardware_class == kCrosSystemUnknownValue)
    machine_info_[kHardwareClassKey] = kUnknownHardwareClass;
  else
    machine_info_[kHardwareClassKey] = hardware_class;

  if (load_oem_manifest) {
    // If kAppOemManifestFile switch is specified, load OEM Manifest file.
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kAppOemManifestFile)) {
      LoadOemManifestFromFile(
          command_line->GetSwitchValuePath(switches::kAppOemManifestFile));
    } else if (base::SysInfo::IsRunningOnChromeOS()) {
      LoadOemManifestFromFile(base::FilePath(kOemManifestFilePath));
    }
    oem_manifest_loaded_ = true;
  }

  if (!base::SysInfo::IsRunningOnChromeOS() &&
      machine_info_.find(kSerialNumber) == machine_info_.end()) {
    // Set stub value for testing. A time value is appended to avoid clashes of
    // the same serial for the same domain, which would invalidate earlier
    // enrollments. A fake /tmp/machine-info file should be used instead if
    // a stable serial is needed, e.g. to test re-enrollment.
    base::TimeDelta time = base::Time::Now() - base::Time::UnixEpoch();
    machine_info_[kSerialNumber] =
        "stub_serial_number_" + base::Int64ToString(time.InSeconds());
  }

  // Finished loading the statistics.
  on_statistics_loaded_.Signal();
  VLOG(1) << "Finished loading statistics.";
}

void StatisticsProviderImpl::LoadOemManifestFromFile(
    const base::FilePath& file) {
  // Called from LoadMachineStatistics. Check cancellation_flag_ again here.
  if (cancellation_flag_.IsSet())
    return;

  KioskOemManifestParser::Manifest oem_manifest;
  if (!KioskOemManifestParser::Load(file, &oem_manifest)) {
    LOG(WARNING) << "Unable to load OEM Manifest file: " << file.value();
    return;
  }
  machine_info_[kOemDeviceRequisitionKey] =
      oem_manifest.device_requisition;
  machine_flags_[kOemIsEnterpriseManagedKey] =
      oem_manifest.enterprise_managed;
  machine_flags_[kOemCanExitEnterpriseEnrollmentKey] =
      oem_manifest.can_exit_enrollment;
  machine_flags_[kOemKeyboardDrivenOobeKey] =
      oem_manifest.keyboard_driven_oobe;

  VLOG(1) << "Loaded OEM Manifest statistics from " << file.value();
}

StatisticsProviderImpl* StatisticsProviderImpl::GetInstance() {
  return Singleton<StatisticsProviderImpl,
                   DefaultSingletonTraits<StatisticsProviderImpl> >::get();
}

static StatisticsProvider* g_test_statistics_provider = NULL;

// static
StatisticsProvider* StatisticsProvider::GetInstance() {
  if (g_test_statistics_provider)
    return g_test_statistics_provider;
  return StatisticsProviderImpl::GetInstance();
}

// static
void StatisticsProvider::SetTestProvider(StatisticsProvider* test_provider) {
  g_test_statistics_provider = test_provider;
}

}  // namespace system
}  // namespace chromeos
