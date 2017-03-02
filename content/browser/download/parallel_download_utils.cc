// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_utils.h"

#include "content/public/browser/download_save_info.h"

namespace content {

DownloadItem::ReceivedSlice FindNextSliceToDownload(
    const std::vector<DownloadItem::ReceivedSlice>& received_slices) {
  if (received_slices.empty())
    return DownloadItem::ReceivedSlice(0, DownloadSaveInfo::kLengthFullContent);

  std::vector<DownloadItem::ReceivedSlice>::const_iterator iter =
      received_slices.begin();
  DCHECK_GE(iter->offset, 0);
  if (iter->offset != 0)
    return DownloadItem::ReceivedSlice(0, iter->offset);

  int64_t offset = 0;
  int64_t remaining_bytes = DownloadSaveInfo::kLengthFullContent;
  while (iter != received_slices.end()) {
    offset = iter->offset + iter->received_bytes;
    std::vector<DownloadItem::ReceivedSlice>::const_iterator next =
        std::next(iter);
    if (next == received_slices.end())
      break;

    DCHECK_GE(next->offset, offset);
    if (next->offset > offset) {
      remaining_bytes = next->offset - offset;
      break;
    }
    iter = next;
  }
  return DownloadItem::ReceivedSlice(offset, remaining_bytes);
}

}  // namespace content
