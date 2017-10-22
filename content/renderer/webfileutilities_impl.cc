// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webfileutilities_impl.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "content/renderer/file_info_util.h"
#include "net/base/filename_util.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebString;

namespace content {

WebFileUtilitiesImpl::WebFileUtilitiesImpl()
    : sandbox_enabled_(true) {
}

WebFileUtilitiesImpl::~WebFileUtilitiesImpl() = default;

bool WebFileUtilitiesImpl::GetFileInfo(const WebString& path,
                                       blink::WebFileInfo& web_file_info) {
  if (sandbox_enabled_) {
    NOTREACHED();
    return false;
  }
  base::File::Info file_info;
  if (!base::GetFileInfo(blink::WebStringToFilePath(path), &file_info))
    return false;

  FileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platform_path = path;
  return true;
}

WebString WebFileUtilitiesImpl::DirectoryName(const WebString& path) {
  return blink::FilePathToWebString(blink::WebStringToFilePath(path).DirName());
}

WebString WebFileUtilitiesImpl::BaseName(const WebString& path) {
  return blink::FilePathToWebString(
      blink::WebStringToFilePath(path).BaseName());
}

blink::WebURL WebFileUtilitiesImpl::FilePathToURL(const WebString& path) {
  return net::FilePathToFileURL(blink::WebStringToFilePath(path));
}

}  // namespace content
