// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/downloads/download_ui_item.h"

namespace offline_pages {

DownloadUIItem::DownloadUIItem()
    : total_bytes(0) {
}

DownloadUIItem::DownloadUIItem(const OfflinePageItem& page)
    : guid(page.client_id.id),
      url(page.url),
      target_path(page.file_path),
      start_time(page.creation_time),
      total_bytes(page.file_size) {}

DownloadUIItem::~DownloadUIItem() {
}

}  // namespace offline_pages
