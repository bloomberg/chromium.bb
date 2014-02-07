// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_policy_connector.h"

#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_info.h"
#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/policy_constants.h"

#if defined(OS_WIN)
#include "components/policy/core/common/policy_loader_win.h"
#elif defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "components/policy/core/common/config_dir_policy_loader.h"
#elif defined(OS_ANDROID)
#include "components/policy/core/common/policy_provider_android.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/system/statistics_provider.h"
#endif

using content::BrowserThread;

namespace policy {

namespace {

// The following constants define delays applied before the initial policy fetch
// on startup. (So that displaying Chrome's GUI does not get delayed.)
// Delay in milliseconds from startup.
const int64 kServiceInitializationStartupDelay = 5000;

#if defined(OS_MACOSX)
base::FilePath GetManagedPolicyPath() {
  // This constructs the path to the plist file in which Mac OS X stores the
  // managed preference for the application. This is undocumented and therefore
  // fragile, but if it doesn't work out, AsyncPolicyLoader has a task that
  // polls periodically in order to reload managed preferences later even if we
  // missed the change.
  base::FilePath path;
  if (!PathService::Get(chrome::DIR_MANAGED_PREFS, &path))
    return base::FilePath();

  CFBundleRef bundle(CFBundleGetMainBundle());
  if (!bundle)
    return base::FilePath();

  CFStringRef bundle_id = CFBundleGetIdentifier(bundle);
  if (!bundle_id)
    return base::FilePath();

  return path.Append(base::SysCFStringRefToUTF8(bundle_id) + ".plist");
}
#endif  // defined(OS_MACOSX)

class DeviceManagementServiceConfiguration
    : public DeviceManagementService::Configuration {
 public:
  DeviceManagementServiceConfiguration() {}
  virtual ~DeviceManagementServiceConfiguration() {}

  virtual std::string GetServerUrl() OVERRIDE {
    return BrowserPolicyConnector::GetDeviceManagementUrl();
  }

  virtual std::string GetAgentParameter() OVERRIDE {
    chrome::VersionInfo version_info;
    return base::StringPrintf("%s %s(%s)",
                              version_info.Name().c_str(),
                              version_info.Version().c_str(),
                              version_info.LastChange().c_str());
  }

  virtual std::string GetPlatformParameter() OVERRIDE {
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
};

}  // namespace

ChromeBrowserPolicyConnector::ChromeBrowserPolicyConnector()
    : BrowserPolicyConnector(base::Bind(&BuildHandlerList)) {
  ConfigurationPolicyProvider* platform_provider = CreatePlatformProvider();
  if (platform_provider)
    SetPlatformPolicyProvider(make_scoped_ptr(platform_provider));
}

ChromeBrowserPolicyConnector::~ChromeBrowserPolicyConnector() {}

void ChromeBrowserPolicyConnector::Init(
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> request_context) {
  // Initialization of some of the providers requires the FILE thread; make
  // sure that threading is ready at this point.
  DCHECK(BrowserThread::IsThreadInitialized(BrowserThread::FILE));

  scoped_ptr<DeviceManagementService::Configuration> configuration(
      new DeviceManagementServiceConfiguration);
  scoped_ptr<DeviceManagementService> device_management_service(
      new DeviceManagementService(configuration.Pass()));
  device_management_service->ScheduleInitialization(
      kServiceInitializationStartupDelay);

  BrowserPolicyConnector::Init(
      local_state, request_context, device_management_service.Pass());
}

ConfigurationPolicyProvider*
    ChromeBrowserPolicyConnector::CreatePlatformProvider() {
#if defined(OS_WIN)
  scoped_ptr<AsyncPolicyLoader> loader(PolicyLoaderWin::Create(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      kRegistryChromePolicyKey));
  return new AsyncPolicyProvider(GetSchemaRegistry(), loader.Pass());
#elif defined(OS_MACOSX)
  scoped_ptr<AsyncPolicyLoader> loader(new PolicyLoaderMac(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      GetManagedPolicyPath(),
      new MacPreferences()));
  return new AsyncPolicyProvider(GetSchemaRegistry(), loader.Pass());
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  base::FilePath config_dir_path;
  if (PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    scoped_ptr<AsyncPolicyLoader> loader(new ConfigDirPolicyLoader(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
        config_dir_path,
        POLICY_SCOPE_MACHINE));
    return new AsyncPolicyProvider(GetSchemaRegistry(), loader.Pass());
  } else {
    return NULL;
  }
#elif defined(OS_ANDROID)
  return new PolicyProviderAndroid();
#else
  return NULL;
#endif
}

}  // namespace policy
