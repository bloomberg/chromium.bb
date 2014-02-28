// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_ATTRIBUTE_CACHE_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_ATTRIBUTE_CACHE_H_

#include <map>

#include "base/files/file.h"
#include "base/values.h"

namespace local_discovery {

class PrivetFileSystemAttributeCache {
 public:
  PrivetFileSystemAttributeCache();
  ~PrivetFileSystemAttributeCache();

  // Get File info for a path in the filesystem. Returns NULL if not found.
  const base::File::Info* GetFileInfo(const base::FilePath& full_path);

  // Add file info from the result of a /privet/storage/list request.
  void AddFileInfoFromJSON(const base::FilePath& full_path,
                           const base::DictionaryValue* json);

 private:
  typedef std::map<base::FilePath, base::File::Info> FileInfoMap;

  void AddEntryInfoFromJSON(const base::FilePath& full_path,
                            const base::DictionaryValue* json);

  FileInfoMap file_info_map_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_ATTRIBUTE_CACHE_H_
