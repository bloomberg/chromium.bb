// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "storage/browser/fileapi/async_file_util.h"

namespace arc {

// Represents a file system root in Android Documents Provider.
//
// All methods must be called on the IO thread.
class ArcDocumentsProviderRoot {
 public:
  using GetFileInfoCallback = storage::AsyncFileUtil::GetFileInfoCallback;
  using ReadDirectoryCallback = storage::AsyncFileUtil::ReadDirectoryCallback;

  ArcDocumentsProviderRoot(const std::string& authority,
                           const std::string& root_document_id);
  ~ArcDocumentsProviderRoot();

  // Queries information of a file just like AsyncFileUtil.GetFileInfo().
  void GetFileInfo(const base::FilePath& path,
                   const GetFileInfoCallback& callback);

  // Queries a list of files under a directory just like
  // AsyncFileUtil.ReadDirectory().
  void ReadDirectory(const base::FilePath& path,
                     const ReadDirectoryCallback& callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcDocumentsProviderRoot);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_
