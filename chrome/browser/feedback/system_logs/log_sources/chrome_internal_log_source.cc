// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"

#include "base/json/json_string_value_serializer.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"


namespace {

const char kSyncDataKey[] = "about_sync_data";
const char kExtensionsListKey[] = "extensions";
const char kChromeVersionTag[] = "CHROME VERSION";
#if !defined(OS_CHROMEOS)
const char kOsVersionTag[] = "OS VERSION";
#endif

}  // namespace

namespace system_logs {

void ChromeInternalLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  SystemLogsResponse response;

  chrome::VersionInfo version_info;
  response[kChromeVersionTag] =  version_info.CreateVersionString();

#if !defined(OS_CHROMEOS)
  // On ChromeOS, this will be pulled in from the LSB_RELEASE.
  std::string os_version = base::SysInfo::OperatingSystemName() + ": " +
                           base::SysInfo::OperatingSystemVersion();
  response[kOsVersionTag] =  os_version;
#endif

  PopulateSyncLogs(&response);
  PopulateExtensionInfoLogs(&response);

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
  scoped_ptr<base::DictionaryValue> sync_logs(
      sync_ui_util::ConstructAboutInformation(service));

  // Remove identity section.
  base::ListValue* details = NULL;
  sync_logs->GetList(kDetailsKey, &details);
  if (!details)
    return;
  for (base::ListValue::iterator it = details->begin();
      it != details->end(); ++it) {
    base::DictionaryValue* dict = NULL;
    if ((*it)->GetAsDictionary(&dict)) {
      std::string title;
      dict->GetString("title", &title);
      if (title == kIdentityTitle) {
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
  if (!ChromeMetricsServiceAccessor::IsCrashReportingEnabled())
    return;

  Profile* primary_profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (!primary_profile)
    return;

  ExtensionService* service =
      extensions::ExtensionSystem::Get(primary_profile)->extension_service();
  if (!service)
    return;

  std::string extensions_list;
  const extensions::ExtensionSet* extensions = service->extensions();
  for (extensions::ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end();
       ++it) {
    const extensions::Extension* extension = it->get();
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

}  // namespace system_logs
