// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/downloads/download_ui_item.h"

#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_item.h"

namespace offline_pages {

DownloadUIItem::DownloadUIItem() : download_progress_bytes(0), total_bytes(0) {}

DownloadUIItem::DownloadUIItem(const OfflinePageItem& page)
    : guid(page.client_id.id),
      url(page.url),
      download_state(DownloadState::COMPLETE),
      download_progress_bytes(0),
      title(page.title),
      target_path(page.file_path),
      start_time(page.creation_time),
      total_bytes(page.file_size) {}

DownloadUIItem::DownloadUIItem(const SavePageRequest& request)
    : guid(request.client_id().id),
      url(request.url()),
      download_progress_bytes(0),  // TODO(dimich) Get this from Request.
      start_time(request.creation_time()),
      total_bytes(-1L) {
  switch (request.request_state()) {
    case SavePageRequest::RequestState::AVAILABLE:
      download_state = DownloadState::PENDING;
      break;
    case SavePageRequest::RequestState::OFFLINING:
      download_state = DownloadState::IN_PROGRESS;
      break;
    case SavePageRequest::RequestState::PAUSED:
      download_state = DownloadState::PAUSED;
      break;
  }
  // TODO(dimich): Fill in download_progress, add change notifications for it.
}

DownloadUIItem::DownloadUIItem(const DownloadUIItem& other)
    : guid(other.guid),
      url(other.url),
      download_state(other.download_state),
      download_progress_bytes(other.download_progress_bytes),
      title(other.title),
      target_path(other.target_path),
      start_time(other.start_time),
      total_bytes(other.total_bytes) {}

DownloadUIItem::~DownloadUIItem() {}

}  // namespace offline_pages
