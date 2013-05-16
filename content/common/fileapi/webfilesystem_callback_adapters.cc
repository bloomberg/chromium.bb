// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/fileapi/webfilesystem_callback_adapters.h"

#include <string>
#include <vector>

#include "base/files/file_util_proxy.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFileInfo;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFileSystemEntry;
using WebKit::WebString;
using WebKit::WebVector;

namespace content {

void FileStatusCallbackAdapter(
    WebKit::WebFileSystemCallbacks* callbacks,
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_OK)
    callbacks->didSucceed();
  else
    callbacks->didFail(::fileapi::PlatformFileErrorToWebFileError(error));
}

void ReadMetadataCallbackAdapter(
    WebKit::WebFileSystemCallbacks* callbacks,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path) {
  WebFileInfo web_file_info;
  webkit_glue::PlatformFileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = webkit_base::FilePathToWebString(platform_path);
  callbacks->didReadMetadata(web_file_info);
}

void CreateSnapshotFileCallbackAdapter(
    WebKit::WebFileSystemCallbacks* callbacks,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path) {
  WebFileInfo web_file_info;
  webkit_glue::PlatformFileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = webkit_base::FilePathToWebString(platform_path);
  callbacks->didCreateSnapshotFile(web_file_info);
}

void ReadDirectoryCallbackAdapater(
    WebKit::WebFileSystemCallbacks* callbacks,
    const std::vector<base::FileUtilProxy::Entry>& entries,
    bool has_more) {
  WebVector<WebFileSystemEntry> file_system_entries(entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    file_system_entries[i].name =
        webkit_base::FilePathStringToWebString(entries[i].name);
    file_system_entries[i].isDirectory = entries[i].is_directory;
  }
  callbacks->didReadDirectory(file_system_entries, has_more);
}

void OpenFileSystemCallbackAdapter(
    WebKit::WebFileSystemCallbacks* callbacks,
    const std::string& name, const GURL& root) {
  callbacks->didOpenFileSystem(UTF8ToUTF16(name), root);
}

}  // namespace content
