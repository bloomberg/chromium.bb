// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/file_system/webfilesystem_callback_dispatcher.h"

#include "base/file_util_proxy.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFileInfo;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFileSystemEntry;
using WebKit::WebString;
using WebKit::WebVector;

namespace {

WebKit::WebFileError PlatformFileErrorToWebFileError(
    base::PlatformFileError error_code) {
  switch (error_code) {
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
      return WebKit::WebFileErrorNotFound;
    case base::PLATFORM_FILE_ERROR_INVALID_OPERATION:
    case base::PLATFORM_FILE_ERROR_EXISTS:
    case base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
      return WebKit::WebFileErrorInvalidModification;
    case base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
      return WebKit::WebFileErrorNoModificationAllowed;
    case base::PLATFORM_FILE_ERROR_FAILED:
      return WebKit::WebFileErrorInvalidState;
    case base::PLATFORM_FILE_ERROR_ABORT:
      return WebKit::WebFileErrorAbort;
    default:
      return WebKit::WebFileErrorInvalidModification;
  }
}

}

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
  callbacks_->didReadMetadata(web_file_info);
}

void WebFileSystemCallbackDispatcher::DidReadDirectory(
    const std::vector<base::file_util_proxy::Entry>& entries, bool has_more) {
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
  callbacks_->didFail(PlatformFileErrorToWebFileError(error_code));
}
