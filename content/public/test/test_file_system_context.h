// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace storage {
class QuotaManagerProxy;
}

namespace storage {
class FileSystemBackend;
}

namespace content {

storage::FileSystemContext* CreateFileSystemContextForTesting(
    storage::QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path);

// The caller is responsible for including TestFileSystemBackend in
// |additional_providers| if needed.
storage::FileSystemContext*
CreateFileSystemContextWithAdditionalProvidersForTesting(
    storage::QuotaManagerProxy* quota_manager_proxy,
    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers,
    const base::FilePath& base_path);

storage::FileSystemContext* CreateFileSystemContextWithAutoMountersForTesting(
    storage::QuotaManagerProxy* quota_manager_proxy,
    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers,
    const std::vector<storage::URLRequestAutoMountHandler>& auto_mounters,
    const base::FilePath& base_path);

storage::FileSystemContext* CreateIncognitoFileSystemContextForTesting(
    storage::QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_CONTEXT_H_
