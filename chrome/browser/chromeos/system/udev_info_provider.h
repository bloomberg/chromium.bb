// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_UDEV_INFO_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_UDEV_INFO_PROVIDER_H_
#pragma once

#include <string>

namespace chromeos {
namespace system {

// Provides access to udev database.
class UdevInfoProvider {
 public:
  static UdevInfoProvider* GetInstance();

  // Query a named device udev property (e.g., "ID_TYPE", "DEVNAME").
  // Returns |true| if property was successfully queried, |false| otherwise.
  // Must be run on the FILE thread.
  virtual bool QueryDeviceProperty(const std::string& sys_path,
                                   const std::string& property_name,
                                   std::string* result) const = 0;

 protected:
  virtual ~UdevInfoProvider() {}
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_UDEV_INFO_PROVIDER_H_
