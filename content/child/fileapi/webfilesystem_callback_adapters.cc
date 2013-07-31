// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/fileapi/webfilesystem_callback_adapters.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebFileSystem.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebFileSystemCallbacks.h"
#include "url/gurl.h"
#include "webkit/common/fileapi/directory_entry.h"
#include "webkit/common/fileapi/file_system_util.h"
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

void OpenFileSystemCallbackAdapter(
    WebKit::WebFileSystemCallbacks* callbacks,
    const std::string& name, const GURL& root) {
  callbacks->didOpenFileSystem(UTF8ToUTF16(name), root);
}

}  // namespace content
