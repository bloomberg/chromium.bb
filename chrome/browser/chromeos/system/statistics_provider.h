// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_

#include <string>

namespace chromeos {
namespace system {

// Developer switch value.
extern const char kDevSwitchBootMode[];

// HWID key.
extern const char kHardwareClass[];

// OEM customization flag that permits exiting enterprise enrollment flow in
// OOBE when 'oem_enterprise_managed' flag is set.
extern const char kOemCanExitEnterpriseEnrollmentKey[];

// OEM customization directive that specified intended device purpose.
extern const char kOemDeviceRequisitionKey[];

// OEM customization flag that enforces enterprise enrollment flow in OOBE.
extern const char kOemIsEnterpriseManagedKey[];

// OEM customization flag that specifies if OOBE flow should be enhanced for
// keyboard driven control.
extern const char kOemKeyboardDrivenOobeKey[];

// Offer coupon code key.
extern const char kOffersCouponCodeKey[];

// Offer group key.
extern const char kOffersGroupCodeKey[];

// This interface provides access to Chrome OS statistics.
class StatisticsProvider {
 public:
  // Initializes the statistics provider.
  virtual void Init() = 0;

  // Starts loading the machine statistcs.
  virtual void StartLoadingMachineStatistics() = 0;

  // Retrieve the named machine statistic (e.g. "hardware_class").
  // This does not update the statistcs. If the |name| is not set, |result|
  // preserves old value.
  virtual bool GetMachineStatistic(const std::string& name,
                                   std::string* result) = 0;

  // Retrieve boolean value for named machine flag.
  virtual bool GetMachineFlag(const std::string& name,
                              bool* result) = 0;

  // Loads kiosk oem manifest file.
  virtual void LoadOemManifest() = 0;

  static StatisticsProvider* GetInstance();

 protected:
  virtual ~StatisticsProvider() {}
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_
