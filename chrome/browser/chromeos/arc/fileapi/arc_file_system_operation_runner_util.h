// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_

#include <string>

#include "components/arc/common/file_system.mojom.h"

class GURL;

namespace arc {
namespace file_system_operation_runner_util {

using GetFileSizeCallback = mojom::FileSystemInstance::GetFileSizeCallback;
using OpenFileToReadCallback =
    mojom::FileSystemInstance::OpenFileToReadCallback;
using GetDocumentCallback = mojom::FileSystemInstance::GetDocumentCallback;
using GetChildDocumentsCallback =
    mojom::FileSystemInstance::GetChildDocumentsCallback;

// Utility functions to post a task to run ArcFileSystemOperationRunner methods.
// These functions must be called on the IO thread.
void GetFileSizeOnIOThread(const GURL& url,
                           const GetFileSizeCallback& callback);
void OpenFileToReadOnIOThread(const GURL& url,
                              const OpenFileToReadCallback& callback);
void GetDocumentOnIOThread(const std::string& authority,
                           const std::string& document_id,
                           const GetDocumentCallback& callback);
void GetChildDocumentsOnIOThread(const std::string& authority,
                                 const std::string& parent_document_id,
                                 const GetChildDocumentsCallback& callback);

}  // namespace file_system_operation_runner_util
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_
