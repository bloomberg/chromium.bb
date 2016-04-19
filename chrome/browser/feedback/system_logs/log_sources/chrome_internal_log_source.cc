// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"

#include "base/json/json_string_value_serializer.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync_driver/about_sync_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#endif

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace {

const char kSyncDataKey[] = "about_sync_data";
const char kExtensionsListKey[] = "extensions";
const char kDataReductionProxyKey[] = "data_reduction_proxy";
const char kChromeVersionTag[] = "CHROME VERSION";
#if defined(OS_CHROMEOS)
const char kChromeEnrollmentTag[] = "ENTERPRISE_ENROLLED";
#else
const char kOsVersionTag[] = "OS VERSION";
#endif
#if defined(OS_WIN)
const char kUsbKeyboardDetected[] = "usb_keyboard_detected";
#endif

#if defined(OS_CHROMEOS)
std::string GetEnrollmentStatusString() {
  switch (ChromeOSMetricsProvider::GetEnrollmentStatus()) {
    case ChromeOSMetricsProvider::NON_MANAGED:
      return "Not managed";
    case ChromeOSMetricsProvider::MANAGED:
      return "Managed";
    case ChromeOSMetricsProvider::UNUSED:
    case ChromeOSMetricsProvider::ERROR_GETTING_ENROLLMENT_STATUS:
    case ChromeOSMetricsProvider::ENROLLMENT_STATUS_MAX:
      return "Error retrieving status";
  }
  // For compilers that don't recognize all cases handled above.
  NOTREACHED();
  return std::string();
}
#endif

}  // namespace

namespace system_logs {

ChromeInternalLogSource::ChromeInternalLogSource()
    : SystemLogsSource("ChromeInternal") {
}

ChromeInternalLogSource::~ChromeInternalLogSource() {
}

void ChromeInternalLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  SystemLogsResponse response;

  response[kChromeVersionTag] = chrome::GetVersionString();

#if defined(OS_CHROMEOS)
  response[kChromeEnrollmentTag] = GetEnrollmentStatusString();
#else
  // On ChromeOS, this will be pulled in from the LSB_RELEASE.
  std::string os_version = base::SysInfo::OperatingSystemName() + ": " +
                           base::SysInfo::OperatingSystemVersion();
  response[kOsVersionTag] =  os_version;
#endif

  PopulateSyncLogs(&response);
  PopulateExtensionInfoLogs(&response);
  PopulateDataReductionProxyLogs(&response);
#if defined(OS_WIN)
  PopulateUsbKeyboardDetected(&response);
#endif

  if (ProfileManager::GetLastUsedProfile()->IsChild())
    response["account_type"] = "child";

  callback.Run(&response);
}

void ChromeInternalLogSource::PopulateSyncLogs(SystemLogsResponse* response) {
  // We are only interested in sync logs for the primary user profile.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile ||
      !ProfileSyncServiceFactory::GetInstance()->HasProfileSyncService(profile))
    return;

  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  std::unique_ptr<base::DictionaryValue> sync_logs(
      sync_driver::sync_ui_util::ConstructAboutInformation(
          service, service->signin(), chrome::GetChannel()));

  // Remove identity section.
  base::ListValue* details = NULL;
  sync_logs->GetList(sync_driver::sync_ui_util::kDetailsKey, &details);
  if (!details)
    return;
  for (base::ListValue::iterator it = details->begin();
      it != details->end(); ++it) {
    base::DictionaryValue* dict = NULL;
    if ((*it)->GetAsDictionary(&dict)) {
      std::string title;
      dict->GetString("title", &title);
      if (title == sync_driver::sync_ui_util::kIdentityTitle) {
        details->Erase(it, NULL);
        break;
      }
    }
  }

  // Add sync logs to logs.
  std::string sync_logs_string;
  JSONStringValueSerializer serializer(&sync_logs_string);
  serializer.Serialize(*sync_logs.get());

  (*response)[kSyncDataKey] = sync_logs_string;
}

void ChromeInternalLogSource::PopulateExtensionInfoLogs(
    SystemLogsResponse* response) {
  if (!ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled())
    return;

  Profile* primary_profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (!primary_profile)
    return;

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(primary_profile);
  std::string extensions_list;
  for (const scoped_refptr<const extensions::Extension>& extension :
       extension_registry->enabled_extensions()) {
    if (extensions_list.empty()) {
      extensions_list = extension->name();
    } else {
      extensions_list += ",\n" + extension->name();
    }
  }
  if (!extensions_list.empty())
    extensions_list += "\n";

  if (!extensions_list.empty())
    (*response)[kExtensionsListKey] = extensions_list;
}

void ChromeInternalLogSource::PopulateDataReductionProxyLogs(
    SystemLogsResponse* response) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  bool is_data_reduction_proxy_enabled =
      prefs->HasPrefPath(prefs::kDataSaverEnabled) &&
      prefs->GetBoolean(prefs::kDataSaverEnabled);
  (*response)[kDataReductionProxyKey] = is_data_reduction_proxy_enabled ?
      "enabled" : "disabled";
}

#if defined(OS_WIN)
void ChromeInternalLogSource::PopulateUsbKeyboardDetected(
    SystemLogsResponse* response) {
  std::string reason;
  bool result = base::win::IsKeyboardPresentOnSlate(&reason);
  (*response)[kUsbKeyboardDetected] = result ? "Keyboard Detected:\n" :
                                               "No Keyboard:\n";
  (*response)[kUsbKeyboardDetected] += reason;
}
#endif

}  // namespace system_logs
