// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/dump_database_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

using sync_file_system::SyncFileSystemServiceFactory;

namespace syncfs_internals {

DumpDatabaseHandler::DumpDatabaseHandler(Profile* profile)
    : profile_(profile) {}
DumpDatabaseHandler::~DumpDatabaseHandler() {}

void DumpDatabaseHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDatabaseDump",
      base::Bind(&DumpDatabaseHandler::GetDatabaseDump,
                 base::Unretained(this)));
}

void DumpDatabaseHandler::GetDatabaseDump(const base::ListValue* args) {
  scoped_ptr<base::ListValue> list =
      SyncFileSystemServiceFactory::GetForProfile(profile_)->DumpDatabase();
  if (!list)
    list.reset(new base::ListValue);
  web_ui()->CallJavascriptFunction("DumpDatabase.onGetDatabaseDump",
                                   *list.get());
}

}  // namespace syncfs_internals
