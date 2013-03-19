// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/chrome_internal_log_source.h"

#include "base/json/json_string_value_serializer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system_logs/system_logs_fetcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/browser_thread.h"


const char kSyncDataKey[] = "about_sync_data";
const char kExtensionsListKey[] = "extensions";

namespace chromeos {

void ChromeInternalLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  SystemLogsResponse response;
  PopulateSyncLogs(&response);
  PopulateExtensionInfoLogs(&response);

  callback.Run(&response);
}

void ChromeInternalLogSource::PopulateSyncLogs(SystemLogsResponse* response) {
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (!ProfileSyncServiceFactory::GetInstance()->HasProfileSyncService(
      profile))
    return;

  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  scoped_ptr<base::DictionaryValue> sync_logs(
      sync_ui_util::ConstructAboutInformation(service));

  // Remove credentials section.
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
      if (title == kCredentialsTitle) {
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
  bool reporting_enabled = false;
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &reporting_enabled);
  if (!reporting_enabled)
    return;

  Profile* default_profile =
      g_browser_process->profile_manager()->GetDefaultProfile();
  if (!default_profile)
    return;

  ExtensionService* service =
      extensions::ExtensionSystem::Get(default_profile)->extension_service();
  if (!service)
    return;

  std::string extensions_list;
  const ExtensionSet* extensions = service->extensions();
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end();
       ++it) {
    const extensions::Extension* extension = *it;
    if (extensions_list.empty()) {
      extensions_list = extension->name();
    } else {
      extensions_list += ", " + extension->name();
    }
  }

  if (!extensions_list.empty())
    (*response)[kExtensionsListKey] = extensions_list;
}

}  // namespace chromeos
