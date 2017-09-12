// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mock_download_item_impl.h"

namespace content {

MockDownloadItemImpl::MockDownloadItemImpl(DownloadItemImplDelegate* delegate)
    : DownloadItemImpl(delegate,
                       std::string("7d122682-55b5-4a47-a253-36cadc3e5bee"),
                       content::DownloadItem::kInvalidId,
                       base::FilePath(),
                       base::FilePath(),
                       std::vector<GURL>(),
                       GURL(),
                       GURL(),
                       GURL(),
                       GURL(),
                       "application/octet-stream",
                       "application/octet-stream",
                       base::Time(),
                       base::Time(),
                       std::string(),
                       std::string(),
                       0,
                       0,
                       std::string(),
                       DownloadItem::COMPLETE,
                       DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                       DOWNLOAD_INTERRUPT_REASON_NONE,
                       false,
                       base::Time(),
                       true,
                       DownloadItem::ReceivedSlices(),
                       net::NetLogWithSource()) {}

MockDownloadItemImpl::~MockDownloadItemImpl() = default;

}  // namespace content
