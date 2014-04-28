// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_service_configuration.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_version_info.h"
#include "components/policy/core/browser/browser_policy_connector.h"

#if defined(OS_CHROMEOS)
#include "chromeos/system/statistics_provider.h"
#endif

namespace policy {

DeviceManagementServiceConfiguration::DeviceManagementServiceConfiguration(
    const std::string& server_url)
    : server_url_(server_url) {
}

DeviceManagementServiceConfiguration::~DeviceManagementServiceConfiguration() {
}

std::string DeviceManagementServiceConfiguration::GetServerUrl() {
  return server_url_;
}

std::string DeviceManagementServiceConfiguration::GetAgentParameter() {
  chrome::VersionInfo version_info;
  return base::StringPrintf("%s %s(%s)",
                            version_info.Name().c_str(),
                            version_info.Version().c_str(),
                            version_info.LastChange().c_str());
}

std::string DeviceManagementServiceConfiguration::GetPlatformParameter() {
  std::string os_name = base::SysInfo::OperatingSystemName();
  std::string os_hardware = base::SysInfo::OperatingSystemArchitecture();

#if defined(OS_CHROMEOS)
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();

  std::string hwclass;
  if (!provider->GetMachineStatistic(chromeos::system::kHardwareClassKey,
                                     &hwclass)) {
    LOG(ERROR) << "Failed to get machine information";
  }
  os_name += ",CrOS," + base::SysInfo::GetLsbReleaseBoard();
  os_hardware += "," + hwclass;
#endif

  std::string os_version("-");
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
  os_version = base::StringPrintf("%d.%d.%d",
                                  os_major_version,
                                  os_minor_version,
                                  os_bugfix_version);
#endif

  return base::StringPrintf(
      "%s|%s|%s", os_name.c_str(), os_hardware.c_str(), os_version.c_str());
}

}  // namespace policy
