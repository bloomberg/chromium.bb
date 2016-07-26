// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_DOWNLOADS_DOWNLOAD_UI_ITEM_H_
#define COMPONENTS_OFFLINE_PAGES_DOWNLOADS_DOWNLOAD_UI_ITEM_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/offline_pages/offline_page_item.h"
#include "url/gurl.h"

namespace offline_pages {

struct DownloadUIItem {
 public:
  DownloadUIItem();
  explicit DownloadUIItem(const OfflinePageItem& page);
  ~DownloadUIItem();

  // Unique id.
  std::string guid;

  // The URL of the captured page.
  GURL url;

  // The file path to the archive with a local copy of the page.
  base::FilePath target_path;

  // The time when the offline archive was created.
  base::Time start_time;

  // The size of the offline copy.
  int64_t total_bytes;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_DOWNLOADS_DOWNLOAD_UI_ITEM_H_
