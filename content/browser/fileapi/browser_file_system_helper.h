// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_BROWSER_FILE_SYSTEM_HELPER_H_
#define CONTENT_BROWSER_FILEAPI_BROWSER_FILE_SYSTEM_HELPER_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "webkit/fileapi/file_system_context.h"

namespace quota {
class SpecialStoragePolicy;
}

// Helper method that returns FileSystemContext constructed for
// the browser process.
CONTENT_EXPORT scoped_refptr<fileapi::FileSystemContext>
CreateFileSystemContext(
    const FilePath& profile_path,
    bool is_incognito,
    quota::SpecialStoragePolicy* special_storage_policy,
    quota::QuotaManagerProxy* quota_manager_proxy);

#endif  // CONTENT_BROWSER_FILEAPI_BROWSER_FILE_SYSTEM_HELPER_H_
