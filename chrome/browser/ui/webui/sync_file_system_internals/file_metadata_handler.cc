// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/file_metadata_handler.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/file_metadata.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/sync_file_system_internals_resources.h"

using sync_file_system::FileMetadata;
using sync_file_system::RemoteFileSyncService;
using sync_file_system::SyncFileSystemServiceFactory;
using sync_file_system::SyncServiceState;

namespace syncfs_internals {

namespace {

std::string FileTypeToString(sync_file_system::FileType type) {
  switch (type) {
    case sync_file_system::TYPE_FILE:
      return "File";
    case sync_file_system::TYPE_FOLDER:
      return "Folder";
  }

  NOTREACHED() << "Unknown FileType: " << type;
  return "Unknown";
}

}  // namespace

FileMetadataHandler::FileMetadataHandler(Profile* profile)
    : profile_(profile),
      weak_factory_(this) {}

FileMetadataHandler::~FileMetadataHandler() {}

void FileMetadataHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getFileMetadata",
      base::Bind(&FileMetadataHandler::GetFileMetadata,
                 base::Unretained(this)));
}

void FileMetadataHandler::GetFileMetadata(
    const base::ListValue* args) {
  OriginFileMetadataMap* metadata_map = new OriginFileMetadataMap;
  size_t* num_results = new size_t(0);
  SyncFileSystemServiceFactory::GetForProfile(profile_)->GetFileMetadataMap(
      metadata_map,
      num_results,
      base::Bind(&FileMetadataHandler::DidGetFileMetadata,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(metadata_map),
                 base::Owned(num_results)));
}

// TODO(calvinlo): This would probably be better if there was a drop down UI
// box to pick one origin at a time. Then this function would only print
// files for one origin.
void FileMetadataHandler::DidGetFileMetadata(
    OriginFileMetadataMap* metadata_map,
    size_t* num_results,
    sync_file_system::SyncStatusCode status) {
  DCHECK(metadata_map);
  DCHECK(num_results);

  // Flatten map hierarchy in initial version.
  base::ListValue list;
  RemoteFileSyncService::OriginFileMetadataMap::const_iterator origin_itr;
  for (origin_itr = metadata_map->begin();
       origin_itr != metadata_map->end();
       ++origin_itr) {
    RemoteFileSyncService::FileMetadataMap::const_iterator file_path_itr;
    for (file_path_itr = origin_itr->second.begin();
         file_path_itr != origin_itr->second.end();
         ++file_path_itr) {
      const GURL& origin = origin_itr->first;
      const FileMetadata& metadata_object = file_path_itr->second;
      std::string status_string = extensions::api::sync_file_system::ToString(
            extensions::SyncFileStatusToExtensionEnum(
                metadata_object.sync_status));

      // Convert each file metadata object into primitives for rendering.
      base::DictionaryValue* dict = new DictionaryValue;
      dict->SetString("origin", origin.spec());
      dict->SetString("status", status_string);
      dict->SetString("type", FileTypeToString(metadata_object.type));
      dict->SetString("title", metadata_object.title);
      dict->SetString("details", metadata_object.service_specific_metadata);
      list.Append(dict);
    }
  }
  web_ui()->CallJavascriptFunction("FileMetadata.onGetFileMetadata", list);
}

}  // namespace syncfs_internals
