// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/file_system/webfilesystem_callback_dispatcher.h"

#include <string>
#include <vector>

#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFileInfo;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFileSystemEntry;
using WebKit::WebString;
using WebKit::WebVector;

WebFileSystemCallbackDispatcher::WebFileSystemCallbackDispatcher(
    WebFileSystemCallbacks* callbacks)
    : callbacks_(callbacks) {
  DCHECK(callbacks_);
}

void WebFileSystemCallbackDispatcher::DidSucceed() {
  callbacks_->didSucceed();
}

void WebFileSystemCallbackDispatcher::DidReadMetadata(
    const base::PlatformFileInfo& file_info) {
  WebFileInfo web_file_info;
  web_file_info.modificationTime = file_info.last_modified.ToDoubleT();
  web_file_info.length = file_info.size;
  if (file_info.is_directory)
    web_file_info.type = WebFileInfo::TypeDirectory;
  else
    web_file_info.type = WebFileInfo::TypeFile;
  callbacks_->didReadMetadata(web_file_info);
}

void WebFileSystemCallbackDispatcher::DidReadDirectory(
    const std::vector<base::FileUtilProxy::Entry>& entries, bool has_more) {
  WebVector<WebFileSystemEntry> file_system_entries(entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    file_system_entries[i].name =
        webkit_glue::FilePathStringToWebString(entries[i].name);
    file_system_entries[i].isDirectory = entries[i].is_directory;
  }
  callbacks_->didReadDirectory(file_system_entries, has_more);
}

void WebFileSystemCallbackDispatcher::DidOpenFileSystem(
    const std::string& name, const FilePath& root_path) {
  callbacks_->didOpenFileSystem(UTF8ToUTF16(name),
                                webkit_glue::FilePathToWebString(root_path));
}

void WebFileSystemCallbackDispatcher::DidFail(
    base::PlatformFileError error_code) {
    callbacks_->didFail(
        webkit_glue::PlatformFileErrorToWebFileError(error_code));
}

void WebFileSystemCallbackDispatcher::DidWrite(int64 bytes, bool complete) {
  NOTREACHED();
}
