// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"

#include <vector>

#include "base/strings/string_util.h"
#include "storage/browser/fileapi/file_system_url.h"

namespace arc {

const char kDocumentsProviderMountPointName[] = "arc-documents-provider";
const base::FilePath::CharType kDocumentsProviderMountPointPath[] =
    "/special/arc-documents-provider";

bool ParseDocumentsProviderUrl(const storage::FileSystemURL& url,
                               std::string* authority,
                               std::string* root_document_id,
                               base::FilePath* path) {
  if (url.type() != storage::kFileSystemTypeArcDocumentsProvider)
    return false;
  base::FilePath url_path_stripped = url.path().StripTrailingSeparators();

  if (!base::FilePath(kDocumentsProviderMountPointPath)
           .IsParent(url_path_stripped)) {
    return false;
  }

  // Filesystem URL format for documents provider is:
  // /special/arc-documents-provider/<authority>/<root_doc_id>/<relative_path>
  std::vector<base::FilePath::StringType> components;
  url_path_stripped.GetComponents(&components);
  if (components.size() < 5)
    return false;

  *authority = components[3];
  *root_document_id = components[4];

  base::FilePath root_path = base::FilePath(kDocumentsProviderMountPointPath)
                                 .Append(*authority)
                                 .Append(*root_document_id);
  // Special case: AppendRelativePath() fails for identical paths.
  if (url_path_stripped == root_path) {
    path->clear();
  } else {
    bool success = root_path.AppendRelativePath(url_path_stripped, path);
    DCHECK(success);
  }
  return true;
}

}  // namespace arc
