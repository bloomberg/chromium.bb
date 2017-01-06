// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for ARC documents provider file system.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_UTIL_H_

#include <string>

#include "base/files/file_path.h"

class GURL;

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace arc {

// The name of ARC documents provider file system mount point.
extern const char kDocumentsProviderMountPointName[];

// The path of ARC documents provider file system mount point.
extern const base::FilePath::CharType kDocumentsProviderMountPointPath[];

// MIME type for directories in Android.
// Defined as DocumentsContract.Document.MIME_TYPE_DIR in Android.
extern const char kAndroidDirectoryMimeType[];

// Parses a FileSystemURL pointing to ARC documents provider file system.
// On success, true is returned. All arguments must not be nullptr.
bool ParseDocumentsProviderUrl(const storage::FileSystemURL& url,
                               std::string* authority,
                               std::string* root_document_id,
                               base::FilePath* path);

// C++ implementation of DocumentsContract.buildDocumentUri() in Android.
GURL BuildDocumentUrl(const std::string& authority,
                      const std::string& document_id);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_UTIL_H_
