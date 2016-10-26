// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_policy_connector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_WIN)
#include "components/policy/core/common/policy_loader_win.h"
#elif defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "components/policy/core/common/config_dir_policy_loader.h"
#elif defined(OS_ANDROID)
#include "components/policy/core/browser/android/android_combined_policy_provider.h"
#endif

using content::BrowserThread;

namespace policy {

namespace {

#if defined(OS_MACOSX)
base::FilePath GetManagedPolicyPath() {
  CFBundleRef bundle(CFBundleGetMainBundle());
  if (!bundle)
    return base::FilePath();

  CFStringRef bundle_id = CFBundleGetIdentifier(bundle);
  if (!bundle_id)
    return base::FilePath();

  return policy::PolicyLoaderMac::GetManagedPolicyPath(bundle_id);
}
#endif  // defined(OS_MACOSX)

}  // namespace

ChromeBrowserPolicyConnector::ChromeBrowserPolicyConnector()
    : BrowserPolicyConnector(base::Bind(&BuildHandlerList)) {
  std::unique_ptr<ConfigurationPolicyProvider> platform_provider =
      CreatePlatformProvider();
  if (platform_provider)
    SetPlatformPolicyProvider(std::move(platform_provider));
}

ChromeBrowserPolicyConnector::~ChromeBrowserPolicyConnector() {}

void ChromeBrowserPolicyConnector::Init(
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> request_context) {
  // Initialization of some of the providers requires the FILE thread; make
  // sure that threading is ready at this point.
  DCHECK(BrowserThread::IsThreadInitialized(BrowserThread::FILE));

  std::unique_ptr<DeviceManagementService::Configuration> configuration(
      new DeviceManagementServiceConfiguration(
          BrowserPolicyConnector::GetDeviceManagementUrl()));
  std::unique_ptr<DeviceManagementService> device_management_service(
      new DeviceManagementService(std::move(configuration)));
  device_management_service->ScheduleInitialization(
      kServiceInitializationStartupDelay);

  InitInternal(local_state, std::move(device_management_service));
}

std::unique_ptr<ConfigurationPolicyProvider>
ChromeBrowserPolicyConnector::CreatePlatformProvider() {
#if defined(OS_WIN)
  std::unique_ptr<AsyncPolicyLoader> loader(PolicyLoaderWin::Create(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE),
      kRegistryChromePolicyKey));
  return base::MakeUnique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#elif defined(OS_MACOSX)
  std::unique_ptr<AsyncPolicyLoader> loader(new PolicyLoaderMac(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE),
      GetManagedPolicyPath(), new MacPreferences()));
  return base::MakeUnique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  base::FilePath config_dir_path;
  if (PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    std::unique_ptr<AsyncPolicyLoader> loader(new ConfigDirPolicyLoader(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE),
        config_dir_path, POLICY_SCOPE_MACHINE));
    return base::MakeUnique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                                 std::move(loader));
  } else {
    return nullptr;
  }
#elif defined(OS_ANDROID)
  return base::MakeUnique<policy::android::AndroidCombinedPolicyProvider>(
      GetSchemaRegistry());
#else
  return nullptr;
#endif
}

}  // namespace policy
