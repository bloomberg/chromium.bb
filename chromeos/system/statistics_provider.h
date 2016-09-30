// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_
#define CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chromeos/chromeos_export.h"

namespace base {
class TaskRunner;
}

namespace chromeos {
namespace system {

// Activation date key.
CHROMEOS_EXPORT extern const char kActivateDateKey[];

// The key that will be present in VPD if the device ever was enrolled.
CHROMEOS_EXPORT extern const char kCheckEnrollmentKey[];

// Customization ID key.
CHROMEOS_EXPORT extern const char kCustomizationIdKey[];

// Developer switch value.
CHROMEOS_EXPORT extern const char kDevSwitchBootKey[];
CHROMEOS_EXPORT extern const char kDevSwitchBootValueVerified[];
CHROMEOS_EXPORT extern const char kDevSwitchBootValueDev[];

// Firmware type and associated values. The values are from crossystem output
// for the mainfw_type key. Normal and developer correspond to Chrome OS
// firmware with MP and developer keys respectively, nonchrome indicates the
// machine doesn't run on Chrome OS firmware. See crossystem source for more
// details.
CHROMEOS_EXPORT extern const char kFirmwareTypeKey[];
CHROMEOS_EXPORT extern const char kFirmwareTypeValueDeveloper[];
CHROMEOS_EXPORT extern const char kFirmwareTypeValueNonchrome[];
CHROMEOS_EXPORT extern const char kFirmwareTypeValueNormal[];

// Serial number key (only VPD v2+ devices). Use GetEnterpriseMachineID() to
// cover legacy devices.
CHROMEOS_EXPORT extern const char kSerialNumberKey[];

// HWID key.
CHROMEOS_EXPORT extern const char kHardwareClassKey[];

// Key/values reporting if Chrome OS is running in a VM or not. These values are
// read from crossystem output. See crossystem source for VM detection logic.
CHROMEOS_EXPORT extern const char kIsVmKey[];
CHROMEOS_EXPORT extern const char kIsVmValueTrue[];
CHROMEOS_EXPORT extern const char kIsVmValueFalse[];

// OEM customization flag that permits exiting enterprise enrollment flow in
// OOBE when 'oem_enterprise_managed' flag is set.
CHROMEOS_EXPORT extern const char kOemCanExitEnterpriseEnrollmentKey[];

// OEM customization directive that specified intended device purpose.
CHROMEOS_EXPORT extern const char kOemDeviceRequisitionKey[];

// OEM customization flag that enforces enterprise enrollment flow in OOBE.
CHROMEOS_EXPORT extern const char kOemIsEnterpriseManagedKey[];

// OEM customization flag that specifies if OOBE flow should be enhanced for
// keyboard driven control.
CHROMEOS_EXPORT extern const char kOemKeyboardDrivenOobeKey[];

// Offer coupon code key.
CHROMEOS_EXPORT extern const char kOffersCouponCodeKey[];

// Offer group key.
CHROMEOS_EXPORT extern const char kOffersGroupCodeKey[];

// Release Brand Code key.
CHROMEOS_EXPORT extern const char kRlzBrandCodeKey[];

// Write protect switch value.
CHROMEOS_EXPORT extern const char kWriteProtectSwitchBootKey[];
CHROMEOS_EXPORT extern const char kWriteProtectSwitchBootValueOff[];
CHROMEOS_EXPORT extern const char kWriteProtectSwitchBootValueOn[];

// Regional data
CHROMEOS_EXPORT extern const char kRegionKey[];
CHROMEOS_EXPORT extern const char kInitialLocaleKey[];
CHROMEOS_EXPORT extern const char kInitialTimezoneKey[];
CHROMEOS_EXPORT extern const char kKeyboardLayoutKey[];

// This interface provides access to Chrome OS statistics.
class CHROMEOS_EXPORT StatisticsProvider {
 public:
  // Starts loading the machine statistics. File operations are performed on
  // |file_task_runner|.
  virtual void StartLoadingMachineStatistics(
      const scoped_refptr<base::TaskRunner>& file_task_runner,
      bool load_oem_manifest) = 0;

  // Returns true if the named machine statistic (e.g. "hardware_class") is
  // found and stores it in |result| (if provided). Probing for the existance of
  // a statistic by setting |result| to nullptr supresses the usual warning in
  // case the statistic is not found. Safe to call from any thread except the
  // task runner passed to Initialize() (e.g. FILE). This may block if called
  // early before the statistics are loaded from disk.
  // StartLoadingMachineStatistics() must be called before this.
  virtual bool GetMachineStatistic(const std::string& name,
                                   std::string* result) = 0;

  // Similar to GetMachineStatistic for boolean flags.
  virtual bool GetMachineFlag(const std::string& name, bool* result) = 0;

  // Returns the machine serial number after examining a set of well-known
  // keys. In case no serial is found (possibly due to the device having already
  // been enrolled or claimed by a local user), an empty string is returned.
  // Caveat: For lumpy, the last letter is ommitted from the serial number for
  // historical reasons.
  // TODO(tnagel): Drop "Enterprise" from the method name and remove lumpy
  // special casing after lumpy EOL.
  std::string GetEnterpriseMachineID();

  // Cancels any pending file operations.
  virtual void Shutdown() = 0;

  // Returns true if the machine is a VM.
  virtual bool IsRunningOnVm() = 0;

  // Get the Singleton instance.
  static StatisticsProvider* GetInstance();

  // Set the instance returned by GetInstance() for testing.
  static void SetTestProvider(StatisticsProvider* test_provider);

 protected:
  virtual ~StatisticsProvider() {}
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_
