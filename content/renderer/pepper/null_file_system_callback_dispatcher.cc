// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/null_file_system_callback_dispatcher.h"

#include "base/files/file_path.h"
#include "base/files/file_util_proxy.h"
#include "base/logging.h"
#include "googleurl/src/gurl.h"

namespace content {

void NullFileSystemCallbackDispatcher::DidSucceed() {
  NOTREACHED();
}

void NullFileSystemCallbackDispatcher::DidReadMetadata(
    const base::PlatformFileInfo& /* file_info */,
    const base::FilePath& /* platform_path */) {
  NOTREACHED();
}

void NullFileSystemCallbackDispatcher::DidCreateSnapshotFile(
    const base::PlatformFileInfo& /* file_info */,
    const base::FilePath& /* platform_path */) {
  NOTREACHED();
}

void NullFileSystemCallbackDispatcher::DidReadDirectory(
    const std::vector<base::FileUtilProxy::Entry>& /* entries */,
    bool /* has_more */) {
  NOTREACHED();
}

void NullFileSystemCallbackDispatcher::DidOpenFileSystem(
    const std::string& /* name */,
    const GURL& /* root */) {
  NOTREACHED();
}

void NullFileSystemCallbackDispatcher::DidWrite(int64 /* bytes */,
                                                bool /* complete */) {
  NOTREACHED();
}

void NullFileSystemCallbackDispatcher::DidOpenFile(
    base::PlatformFile /* file */,
    int /* file_open_id */,
    quota::QuotaLimitType /* quota_policy */) {
  NOTREACHED();
}

void NullFileSystemCallbackDispatcher::DidFail(
    base::PlatformFileError /* platform_error */) {
  NOTREACHED();
}

}  // namespace content
