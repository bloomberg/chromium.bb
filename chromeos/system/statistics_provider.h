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

// HWID key.
CHROMEOS_EXPORT extern const char kHardwareClassKey[];

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

// This interface provides access to Chrome OS statistics.
class CHROMEOS_EXPORT StatisticsProvider {
 public:
  // Starts loading the machine statistics. File operations are performed on
  // |file_task_runner|.
  virtual void StartLoadingMachineStatistics(
      const scoped_refptr<base::TaskRunner>& file_task_runner,
      bool load_oem_manifest) = 0;

  // Retrieves the named machine statistic (e.g. "hardware_class"). If |name|
  // is found, sets |result| and returns true. Safe to call from any thread
  // except the task runner passed to Initialize() (e.g. FILE). This may block
  // if called early before the statistics are loaded from disk.
  // StartLoadingMachineStatistics() must be called before this.
  virtual bool GetMachineStatistic(const std::string& name,
                                   std::string* result) = 0;

  // Checks whether a machine statistic is present.
  virtual bool HasMachineStatistic(const std::string& name) = 0;

  // Similar to GetMachineStatistic for boolean flags.
  virtual bool GetMachineFlag(const std::string& name, bool* result) = 0;

  // Checks whether a machine flag is present.
  virtual bool HasMachineFlag(const std::string& name) = 0;

  // Cancels any pending file operations.
  virtual void Shutdown() = 0;

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
