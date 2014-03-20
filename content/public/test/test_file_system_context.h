// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "webkit/browser/fileapi/file_system_context.h"

namespace quota {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace fileapi {
class FileSystemBackend;
}

namespace content {

fileapi::FileSystemContext* CreateFileSystemContextForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path);

// The caller is responsible for including TestFileSystemBackend in
// |additional_providers| if needed.
fileapi::FileSystemContext*
CreateFileSystemContextWithAdditionalProvidersForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    ScopedVector<fileapi::FileSystemBackend> additional_providers,
    const base::FilePath& base_path);

fileapi::FileSystemContext*
CreateFileSystemContextWithAutoMountersForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    ScopedVector<fileapi::FileSystemBackend> additional_providers,
    const std::vector<fileapi::URLRequestAutoMountHandler>& auto_mounters,
    const base::FilePath& base_path);

fileapi::FileSystemContext* CreateIncognitoFileSystemContextForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_CONTEXT_H_
