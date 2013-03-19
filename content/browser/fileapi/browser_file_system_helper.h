// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_BROWSER_FILE_SYSTEM_HELPER_H_
#define CONTENT_BROWSER_FILEAPI_BROWSER_FILE_SYSTEM_HELPER_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "webkit/fileapi/file_system_context.h"

namespace fileapi {
class ExternalMountPoints;
class FileSystemContext;
class FileSystemURL;
}

namespace quota {
class SpecialStoragePolicy;
}

namespace content {

// Helper method that returns FileSystemContext constructed for
// the browser process.
CONTENT_EXPORT scoped_refptr<fileapi::FileSystemContext>
CreateFileSystemContext(
    const base::FilePath& profile_path,
    bool is_incognito,
    fileapi::ExternalMountPoints* external_mount_points,
    quota::SpecialStoragePolicy* special_storage_policy,
    quota::QuotaManagerProxy* quota_manager_proxy);

// Check whether a process has permission to access the file system URL.
CONTENT_EXPORT bool CheckFileSystemPermissionsForProcess(
    fileapi::FileSystemContext* context,
    int process_id,
    const fileapi::FileSystemURL& url,
    int permissions,
    base::PlatformFileError* error);

// Get the platform path from a file system URL. This needs to be called
// on the FILE thread.
CONTENT_EXPORT void SyncGetPlatformPath(fileapi::FileSystemContext* context,
                                        int process_id,
                                        const GURL& path,
                                        base::FilePath* platform_path);
}  // namespace content

#endif  // CONTENT_BROWSER_FILEAPI_BROWSER_FILE_SYSTEM_HELPER_H_
