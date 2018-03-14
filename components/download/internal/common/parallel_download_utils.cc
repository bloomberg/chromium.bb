// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/parallel_download_utils.h"

namespace download {

namespace {

// TODO(qinmin): replace this with a comparator operator in
// DownloadItem::ReceivedSlice.
bool compareReceivedSlices(const DownloadItem::ReceivedSlice& lhs,
                           const DownloadItem::ReceivedSlice& rhs) {
  return lhs.offset < rhs.offset;
}

}  // namespace

std::vector<DownloadItem::ReceivedSlice> FindSlicesToDownload(
    const std::vector<DownloadItem::ReceivedSlice>& received_slices) {
  std::vector<DownloadItem::ReceivedSlice> result;
  if (received_slices.empty()) {
    result.emplace_back(0, DownloadSaveInfo::kLengthFullContent);
    return result;
  }

  std::vector<DownloadItem::ReceivedSlice>::const_iterator iter =
      received_slices.begin();
  DCHECK_GE(iter->offset, 0);
  if (iter->offset != 0)
    result.emplace_back(0, iter->offset);

  while (true) {
    int64_t offset = iter->offset + iter->received_bytes;
    std::vector<DownloadItem::ReceivedSlice>::const_iterator next =
        std::next(iter);
    if (next == received_slices.end()) {
      result.emplace_back(offset, DownloadSaveInfo::kLengthFullContent);
      break;
    }

    DCHECK_GE(next->offset, offset);
    if (next->offset > offset)
      result.emplace_back(offset, next->offset - offset);
    iter = next;
  }
  return result;
}

size_t AddOrMergeReceivedSliceIntoSortedArray(
    const DownloadItem::ReceivedSlice& new_slice,
    std::vector<DownloadItem::ReceivedSlice>& received_slices) {
  std::vector<DownloadItem::ReceivedSlice>::iterator it =
      std::upper_bound(received_slices.begin(), received_slices.end(),
                       new_slice, compareReceivedSlices);
  if (it != received_slices.begin()) {
    std::vector<DownloadItem::ReceivedSlice>::iterator prev = std::prev(it);
    if (prev->offset + prev->received_bytes == new_slice.offset) {
      prev->received_bytes += new_slice.received_bytes;
      return static_cast<size_t>(std::distance(received_slices.begin(), prev));
    }
  }

  it = received_slices.emplace(it, new_slice);
  return static_cast<size_t>(std::distance(received_slices.begin(), it));
}

bool CanRecoverFromError(
    const DownloadFileImpl::SourceStream* error_stream,
    const DownloadFileImpl::SourceStream* preceding_neighbor) {
  DCHECK(error_stream->offset() >= preceding_neighbor->offset())
      << "Preceding"
         "stream's offset should be smaller than the error stream.";
  DCHECK_GE(error_stream->length(), 0);

  if (preceding_neighbor->is_finished()) {
    // Check if the preceding stream fetched to the end of the file without
    // error. The error stream doesn't need to download anything.
    if (preceding_neighbor->length() == DownloadSaveInfo::kLengthFullContent &&
        preceding_neighbor->GetCompletionStatus() ==
            DOWNLOAD_INTERRUPT_REASON_NONE) {
      return true;
    }

    // Check if finished preceding stream has already downloaded all data for
    // the error stream.
    if (error_stream->length() > 0) {
      return error_stream->offset() + error_stream->length() <=
             preceding_neighbor->offset() + preceding_neighbor->bytes_written();
    }

    return false;
  }

  // If preceding stream is half open, and still working, we can recover.
  if (preceding_neighbor->length() == DownloadSaveInfo::kLengthFullContent) {
    return true;
  }

  // Check if unfinished preceding stream is able to download data for error
  // stream in the future only when preceding neighbor and error stream both
  // have an upper bound.
  if (error_stream->length() > 0 && preceding_neighbor->length() > 0) {
    return error_stream->offset() + error_stream->length() <=
           preceding_neighbor->offset() + preceding_neighbor->length();
  }

  return false;
}

void DebugSlicesInfo(const DownloadItem::ReceivedSlices& slices) {
  DVLOG(1) << "Received slices size : " << slices.size();
  for (const auto& it : slices) {
    DVLOG(1) << "Slice offset = " << it.offset
             << " , received_bytes = " << it.received_bytes
             << " , finished = " << it.finished;
  }
}

}  // namespace download
