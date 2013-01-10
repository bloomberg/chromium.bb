// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/fileapi/webfilesystem_callback_dispatcher.h"

#include <string>
#include <vector>

#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileInfo.h"
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

WebFileSystemCallbackDispatcher::WebFileSystemCallbackDispatcher(
    WebFileSystemCallbacks* callbacks)
    : callbacks_(callbacks) {
  DCHECK(callbacks_);
}

void WebFileSystemCallbackDispatcher::DidSucceed() {
  callbacks_->didSucceed();
}

void WebFileSystemCallbackDispatcher::DidReadMetadata(
    const base::PlatformFileInfo& file_info, const FilePath& platform_path) {
  WebFileInfo web_file_info;
  webkit_glue::PlatformFileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = webkit_base::FilePathToWebString(platform_path);
  callbacks_->didReadMetadata(web_file_info);
}

void WebFileSystemCallbackDispatcher::DidReadDirectory(
    const std::vector<base::FileUtilProxy::Entry>& entries, bool has_more) {
  WebVector<WebFileSystemEntry> file_system_entries(entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    file_system_entries[i].name =
        webkit_base::FilePathStringToWebString(entries[i].name);
    file_system_entries[i].isDirectory = entries[i].is_directory;
  }
  callbacks_->didReadDirectory(file_system_entries, has_more);
}

void WebFileSystemCallbackDispatcher::DidOpenFileSystem(
    const std::string& name, const GURL& root) {
  callbacks_->didOpenFileSystem(UTF8ToUTF16(name), root);
}

void WebFileSystemCallbackDispatcher::DidFail(
    base::PlatformFileError error_code) {
    callbacks_->didFail(
        fileapi::PlatformFileErrorToWebFileError(error_code));
}

void WebFileSystemCallbackDispatcher::DidWrite(int64 bytes, bool complete) {
  NOTREACHED();
}

}  // namespace content
