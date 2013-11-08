// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/extension_statuses_handler.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/sync_file_system_internals_resources.h"

using sync_file_system::SyncFileSystemServiceFactory;
using sync_file_system::SyncServiceState;

namespace syncfs_internals {

ExtensionStatusesHandler::ExtensionStatusesHandler(Profile* profile)
    : profile_(profile) {}

ExtensionStatusesHandler::~ExtensionStatusesHandler() {}

void ExtensionStatusesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getExtensionStatuses",
      base::Bind(&ExtensionStatusesHandler::GetExtensionStatuses,
                 base::Unretained(this)));
}

//static
void ExtensionStatusesHandler::GetExtensionStatusesAsDictionary(
    Profile* profile,
    base::ListValue* values) {
  DCHECK(profile);
  DCHECK(values);
  std::map<GURL, std::string> status_map;
  SyncFileSystemServiceFactory::GetForProfile(profile)->GetExtensionStatusMap(
      &status_map);

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service)
    return;
  for (std::map<GURL, std::string>::const_iterator itr = status_map.begin();
       itr != status_map.end();
       ++itr) {
    std::string extension_id = itr->first.HostNoBrackets();

    // Join with human readable extension name.
    const extensions::Extension* extension =
        extension_service->GetExtensionById(extension_id, true);
    if (!extension)
      continue;

    base::DictionaryValue* dict = new DictionaryValue;
    dict->SetString("extensionID", extension_id);
    dict->SetString("extensionName", extension->name());
    dict->SetString("status", itr->second);
    values->Append(dict);
  }
}

void ExtensionStatusesHandler::GetExtensionStatuses(
    const base::ListValue* args) {
  DCHECK(args);
  base::ListValue list;
  GetExtensionStatusesAsDictionary(profile_, &list);
  web_ui()->CallJavascriptFunction("ExtensionStatuses.onGetExtensionStatuses",
                                   list);
}

}  // namespace syncfs_internals
