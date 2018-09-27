// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/offline_item_utils.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

using DownloadItem = download::DownloadItem;
using ContentId = offline_items_collection::ContentId;
using OfflineItem = offline_items_collection::OfflineItem;
using OfflineItemFilter = offline_items_collection::OfflineItemFilter;
using OfflineItemState = offline_items_collection::OfflineItemState;
using OfflineItemProgressUnit =
    offline_items_collection::OfflineItemProgressUnit;
using PendingState = offline_items_collection::PendingState;

namespace {

// The namespace for downloads.
const char kDownloadNamespace[] = "LEGACY_DOWNLOAD";

// The namespace for incognito downloads.
const char kDownloadIncognitoNamespace[] = "LEGACY_DOWNLOAD_INCOGNITO";

// The remaining time for a download item if it cannot be calculated.
constexpr int64_t kUnknownRemainingTime = -1;

OfflineItemFilter MimeTypeToOfflineItemFilter(const std::string& mime_type) {
  OfflineItemFilter filter = OfflineItemFilter::FILTER_OTHER;

  if (base::StartsWith(mime_type, "audio/", base::CompareCase::SENSITIVE)) {
    filter = OfflineItemFilter::FILTER_AUDIO;
  } else if (base::StartsWith(mime_type, "video/",
                              base::CompareCase::SENSITIVE)) {
    filter = OfflineItemFilter::FILTER_VIDEO;
  } else if (base::StartsWith(mime_type, "image/",
                              base::CompareCase::SENSITIVE)) {
    filter = OfflineItemFilter::FILTER_IMAGE;
  } else if (base::StartsWith(mime_type, "text/",
                              base::CompareCase::SENSITIVE)) {
    filter = OfflineItemFilter::FILTER_DOCUMENT;
  } else {
    filter = OfflineItemFilter::FILTER_OTHER;
  }

  return filter;
}

}  // namespace

OfflineItem OfflineItemUtils::CreateOfflineItem(DownloadItem* download_item) {
  bool off_the_record =
      content::DownloadItemUtils::GetBrowserContext(download_item)
          ->IsOffTheRecord();

  OfflineItem item;
  item.id =
      ContentId(GetDownloadNamespace(off_the_record), download_item->GetGuid());
  item.title = download_item->GetFileNameToReportUser().AsUTF8Unsafe();
  item.description = download_item->GetFileNameToReportUser().AsUTF8Unsafe();
  item.filter = MimeTypeToOfflineItemFilter(download_item->GetMimeType());
  item.is_transient = download_item->IsTransient();
  item.is_suggested = false;
  item.is_accelerated = download_item->IsParallelDownload();

  item.total_size_bytes = download_item->GetTotalBytes();
  item.externally_removed = download_item->GetFileExternallyRemoved();
  item.creation_time = download_item->GetStartTime();
  item.last_accessed_time = download_item->GetLastAccessTime();
  item.is_openable = download_item->CanOpenDownload() &&
                     blink::IsSupportedMimeType(download_item->GetMimeType());
  item.file_path = download_item->GetTargetFilePath();
  item.mime_type = download_item->GetMimeType();

  item.page_url = download_item->GetTabUrl();
  item.original_url = download_item->GetOriginalUrl();
  item.is_off_the_record = off_the_record;

  item.is_resumable = download_item->CanResume();
  item.received_bytes = download_item->GetReceivedBytes();
  item.is_dangerous = download_item->IsDangerous();

  base::TimeDelta time_delta;
  bool time_remaining_known = download_item->TimeRemaining(&time_delta);
  item.time_remaining_ms = time_remaining_known ? time_delta.InMilliseconds()
                                                : kUnknownRemainingTime;
  // TODO(crbug.com/857549): Add allow_metered, fail_state, pending_state.

  switch (download_item->GetState()) {
    case DownloadItem::IN_PROGRESS:
      item.state = download_item->IsPaused() ? OfflineItemState::PAUSED
                                             : OfflineItemState::IN_PROGRESS;
      break;
    case DownloadItem::COMPLETE:
      item.state = download_item->GetReceivedBytes() == 0
                       ? OfflineItemState::FAILED
                       : OfflineItemState::COMPLETE;
      break;
    case DownloadItem::CANCELLED:
      item.state = OfflineItemState::CANCELLED;
      break;
    case DownloadItem::INTERRUPTED:
      item.state = OfflineItemState::INTERRUPTED;
      break;
    default:
      NOTREACHED();
  }

  item.progress.value = download_item->GetReceivedBytes();
  if (download_item->PercentComplete() != -1)
    item.progress.max = download_item->GetTotalBytes();

  item.progress.unit = OfflineItemProgressUnit::BYTES;

  return item;
}

std::string OfflineItemUtils::GetDownloadNamespace(bool is_off_the_record) {
  return is_off_the_record ? kDownloadIncognitoNamespace : kDownloadNamespace;
}
