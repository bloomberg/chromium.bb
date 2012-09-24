// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file_factory.h"

#include "content/browser/download/download_file_impl.h"
#include "content/browser/power_save_blocker.h"

namespace content {

DownloadFileFactory::~DownloadFileFactory() {}

DownloadFile* DownloadFileFactory::CreateFile(
    DownloadCreateInfo* info,
    scoped_ptr<content::ByteStreamReader> stream,
    DownloadManager* download_manager,
    bool calculate_hash,
    const net::BoundNetLog& bound_net_log) {
  return new DownloadFileImpl(
      info, stream.Pass(), new DownloadRequestHandle(info->request_handle),
      download_manager, calculate_hash,
      scoped_ptr<content::PowerSaveBlocker>(
          new content::PowerSaveBlocker(
              content::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
              "Download in progress")).Pass(),
      bound_net_log);
}

}  // namespace content
