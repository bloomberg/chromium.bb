// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/extension_statuses_handler.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
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

void ExtensionStatusesHandler::GetExtensionStatuses(
    const base::ListValue* args) {
  DCHECK(args);
  std::map<GURL, std::string> status_map;
  SyncFileSystemServiceFactory::GetForProfile(profile_)->GetExtensionStatusMap(
      &status_map);

  base::ListValue list;
  for (std::map<GURL, std::string>::const_iterator itr = status_map.begin();
       itr != status_map.end();
       ++itr) {
    base::DictionaryValue* dict = new DictionaryValue;
    dict->SetString("extensionID", itr->first.spec());
    dict->SetString("status", itr->second);
    list.Append(dict);
  }

  web_ui()->CallJavascriptFunction("ExtensionStatuses.onGetExtensionStatuses",
                                   list);
}

}  // namespace syncfs_internals
