// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/storage/privet_filesystem_attribute_cache.h"

#include "chrome/browser/local_discovery/storage/path_util.h"
#include "chrome/browser/local_discovery/storage/privet_filesystem_constants.h"

namespace local_discovery {

PrivetFileSystemAttributeCache::PrivetFileSystemAttributeCache() {}

PrivetFileSystemAttributeCache::~PrivetFileSystemAttributeCache() {}

const base::File::Info* PrivetFileSystemAttributeCache::GetFileInfo(
    const base::FilePath& full_path) {
  FileInfoMap::iterator found =
      file_info_map_.find(NormalizeFilePath(full_path));

  if (found != file_info_map_.end()) {
    return &found->second;
  }

  return NULL;
}

void PrivetFileSystemAttributeCache::AddFileInfoFromJSON(
    const base::FilePath& full_path,
    const base::DictionaryValue* json) {
  AddEntryInfoFromJSON(full_path, json);

  const base::ListValue* entry_list;
  if (!json->GetList(kPrivetListEntries, &entry_list))
    return;

  for (size_t i = 0; i < entry_list->GetSize(); i++) {
    const base::DictionaryValue* entry_value;
    if (!entry_list->GetDictionary(i, &entry_value))
      break;

    std::string name;
    if (!entry_value->GetString(kPrivetListKeyName, &name))
      break;

    AddEntryInfoFromJSON(full_path.AppendASCII(name), entry_value);
  }
}

void PrivetFileSystemAttributeCache::AddEntryInfoFromJSON(
    const base::FilePath& full_path,
    const base::DictionaryValue* json) {
  base::File::Info file_info;

  std::string type;
  int size = 0;

  json->GetString(kPrivetListKeyType, &type);
  json->GetInteger(kPrivetListKeySize, &size);

  file_info.size = size;
  file_info.is_directory = (type == kPrivetListTypeDir);
  file_info.is_symbolic_link = false;

  file_info_map_[NormalizeFilePath(full_path)] = file_info;
}

}  // namespace local_discovery
