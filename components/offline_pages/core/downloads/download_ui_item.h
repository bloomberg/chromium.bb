// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_DOWNLOADS_DOWNLOAD_UI_ITEM_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_DOWNLOADS_DOWNLOAD_UI_ITEM_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace offline_pages {

struct OfflinePageItem;
class SavePageRequest;

// The abstract "download item" that may be a media file, a web page (together
// with all the resources) or a PWA web package. This is a data bag that exposes
// only bits potentially visible by the user, not the internal data.
struct DownloadUIItem {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offlinepages.downloads
  enum DownloadState { PENDING = 0, IN_PROGRESS = 1, PAUSED = 2, COMPLETE = 3 };

  DownloadUIItem();
  explicit DownloadUIItem(const OfflinePageItem& page);
  explicit DownloadUIItem(const SavePageRequest& request);
  DownloadUIItem(const DownloadUIItem& other);
  ~DownloadUIItem();

  // Unique ID.
  std::string guid;

  // The URL of the captured page.
  GURL url;

  DownloadState download_state;

  int64_t download_progress_bytes;

  // The Title of the captured page, if any. It can be empty string either
  // because the page is not yet fully loaded, or because it doesn't have any.
  base::string16 title;

  // The file path to the archive with a local copy of the page.
  base::FilePath target_path;

  // The time when the offline archive was created.
  base::Time start_time;

  // The size of the offline copy.
  int64_t total_bytes;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_DOWNLOADS_DOWNLOAD_UI_ITEM_H_
