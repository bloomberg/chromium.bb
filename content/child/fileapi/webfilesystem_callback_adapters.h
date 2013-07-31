// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_CALLBACK_ADAPTERS_H_
#define CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_CALLBACK_ADAPTERS_H_

#include "base/basictypes.h"
#include "base/platform_file.h"

class GURL;

namespace fileapi {
struct DirectoryEntry;
}

namespace WebKit {
class WebFileSystemCallbacks;
}

namespace content {

void FileStatusCallbackAdapter(
    WebKit::WebFileSystemCallbacks* callbacks,
    base::PlatformFileError error);

void ReadMetadataCallbackAdapter(
    WebKit::WebFileSystemCallbacks* callbacks,
    const base::PlatformFileInfo& file_info);

void CreateSnapshotFileCallbackAdapter(
    WebKit::WebFileSystemCallbacks* callbacks,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path);

void ReadDirectoryCallbackAdapater(
    WebKit::WebFileSystemCallbacks* callbacks,
    const std::vector<fileapi::DirectoryEntry>& entries,
    bool has_more);

void OpenFileSystemCallbackAdapter(
    WebKit::WebFileSystemCallbacks* callbacks,
    const std::string& name, const GURL& root);

}  // namespace content

#endif  // CONTENT_CHILD_FILEAPI_WEBFILESYSTEM_CALLBACK_ADAPTERS_H_
