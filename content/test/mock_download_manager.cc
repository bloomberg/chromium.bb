// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_download_manager.h"

#include "content/browser/download/byte_stream.h"
#include "content/browser/download/download_create_info.h"

void PrintTo(const DownloadRequestHandle& params, std::ostream* os) {
}

namespace content {

MockDownloadManager::MockDownloadManager() {}

MockDownloadManager::~MockDownloadManager() {}

content::DownloadId MockDownloadManager::StartDownload(
    scoped_ptr<DownloadCreateInfo> info,
    scoped_ptr<content::ByteStreamReader> stream) {
  return MockStartDownload(info.get(), stream.get());
}

}  // namespace content
