// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file_factory.h"

#include <utility>

#include "content/browser/download/download_file_impl.h"

namespace content {

DownloadFileFactory::~DownloadFileFactory() {}

DownloadFile* DownloadFileFactory::CreateFile(
    const DownloadSaveInfo& save_info,
    const base::FilePath& default_downloads_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    base::File file,
    scoped_ptr<ByteStreamReader> byte_stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  return new DownloadFileImpl(save_info, default_downloads_directory, url,
                              referrer_url, calculate_hash, std::move(file),
                              std::move(byte_stream), bound_net_log, observer);
}

}  // namespace content
