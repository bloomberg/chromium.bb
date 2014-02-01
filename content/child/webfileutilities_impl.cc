// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webfileutilities_impl.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "content/child/blink_glue.h"
#include "net/base/file_stream.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebString;

namespace content {

WebFileUtilitiesImpl::WebFileUtilitiesImpl()
    : sandbox_enabled_(true) {
}

WebFileUtilitiesImpl::~WebFileUtilitiesImpl() {
}

bool WebFileUtilitiesImpl::getFileInfo(const WebString& path,
                                       blink::WebFileInfo& web_file_info) {
  if (sandbox_enabled_) {
    NOTREACHED();
    return false;
  }
  // TODO(rvargas): convert this code to use base::File::Info.
  base::File::Info file_info;
  if (!base::GetFileInfo(base::FilePath::FromUTF16Unsafe(path),
                         reinterpret_cast<base::File::Info*>(&file_info)))
    return false;

  FileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = path;
  return true;
}

WebString WebFileUtilitiesImpl::directoryName(const WebString& path) {
  return base::FilePath::FromUTF16Unsafe(path).DirName().AsUTF16Unsafe();
}

WebString WebFileUtilitiesImpl::baseName(const WebString& path) {
  return base::FilePath::FromUTF16Unsafe(path).BaseName().AsUTF16Unsafe();
}

blink::WebURL WebFileUtilitiesImpl::filePathToURL(const WebString& path) {
  return net::FilePathToFileURL(base::FilePath::FromUTF16Unsafe(path));
}

base::PlatformFile WebFileUtilitiesImpl::openFile(const WebString& path,
                                                  int mode) {
  if (sandbox_enabled_) {
    NOTREACHED();
    return base::kInvalidPlatformFileValue;
  }
  // mode==0 (read-only) is the only supported mode.
  // TODO(kinuko): Remove this parameter.
  DCHECK_EQ(0, mode);
  return base::CreatePlatformFile(
      base::FilePath::FromUTF16Unsafe(path),
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL, NULL);
}

void WebFileUtilitiesImpl::closeFile(base::PlatformFile& handle) {
  if (handle == base::kInvalidPlatformFileValue)
    return;
  if (base::ClosePlatformFile(handle))
    handle = base::kInvalidPlatformFileValue;
}

int WebFileUtilitiesImpl::readFromFile(base::PlatformFile handle,
                                       char* data,
                                       int length) {
  if (handle == base::kInvalidPlatformFileValue || !data || length <= 0)
    return -1;
  return base::ReadPlatformFileCurPosNoBestEffort(handle, data, length);
}

}  // namespace content
