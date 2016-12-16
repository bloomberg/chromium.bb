// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_MAP_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_MAP_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/macros.h"

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace arc {

class ArcDocumentsProviderRoot;

// Container of ArcDocumentsProviderRoot instances.
//
// This class is thread-safe, but ArcDocumentsProviderRoot must be accessed
// only on the IO thread anyway.
class ArcDocumentsProviderRootMap {
 public:
  ArcDocumentsProviderRootMap();
  ~ArcDocumentsProviderRootMap();

  // Looks up a root corresponding to |url|.
  // |path| is set to the remaining path part of |url|.
  // Returns nullptr if |url| is invalid or no corresponding root is registered.
  ArcDocumentsProviderRoot* ParseAndLookup(const storage::FileSystemURL& url,
                                           base::FilePath* path) const;

 private:
  // Key is (authority, root_document_id).
  using Key = std::pair<std::string, std::string>;
  std::map<Key, std::unique_ptr<ArcDocumentsProviderRoot>> map_;

  DISALLOW_COPY_AND_ASSIGN(ArcDocumentsProviderRootMap);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_MAP_H_
