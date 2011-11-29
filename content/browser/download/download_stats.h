// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Holds helpers for gathering UMA stats about downloads.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/browser/download/interrupt_reasons.h"

namespace base {
class Time;
class TimeTicks;
}

namespace download_stats {

// We keep a count of how often various events occur in the
// histogram "Download.Counts".
enum DownloadCountTypes {
  // The download was initiated by navigating to a URL (e.g. by user
  // click).
  INITIATED_BY_NAVIGATION_COUNT = 0,

  // The download was initiated by invoking a context menu within a page.
  INITIATED_BY_CONTEXT_MENU_COUNT,

  // The download was initiated when the SavePackage system rejected
  // a Save Page As ... by returning false from
  // SavePackage::IsSaveableContents().
  INITIATED_BY_SAVE_PACKAGE_FAILURE_COUNT,

  // The download was initiated by a drag and drop from a drag-and-drop
  // enabled web application.
  INITIATED_BY_DRAG_N_DROP_COUNT,

  // The download was initiated by explicit RPC from the renderer process
  // (e.g. by Alt-click).
  INITIATED_BY_RENDERER_COUNT,

  // Downloads that made it to DownloadResourceHandler -- all of the
  // above minus those blocked by DownloadThrottlingResourceHandler.
  UNTHROTTLED_COUNT,

  // Downloads that actually complete.
  COMPLETED_COUNT,

  // Downloads that are cancelled before completion (user action or error).
  CANCELLED_COUNT,

  // Downloads that are started. Should be equal to UNTHROTTLED_COUNT.
  START_COUNT,

  // Downloads that were interrupted by the OS.
  INTERRUPTED_COUNT,

  // Write sizes for downloads.
  WRITE_SIZE_COUNT,

  // Counts iterations of the BaseFile::AppendDataToFile() loop.
  WRITE_LOOP_COUNT,

  // Counts interruptions that happened at the end of the download.
  INTERRUPTED_AT_END_COUNT,

  DOWNLOAD_COUNT_TYPES_LAST_ENTRY
};

// Increment one of the above counts.
CONTENT_EXPORT void RecordDownloadCount(DownloadCountTypes type);

// Record COMPLETED_COUNT and how long the download took.
void RecordDownloadCompleted(const base::TimeTicks& start, int64 download_len);

// Record INTERRUPTED_COUNT, |reason|, |received| and |total| bytes.
void RecordDownloadInterrupted(InterruptReason reason,
                               int64 received,
                               int64 total);

// Records the mime type of the download.
void RecordDownloadMimeType(const std::string& mime_type);

// Record WRITE_SIZE_COUNT and data_len.
void RecordDownloadWriteSize(size_t data_len);

// Record WRITE_LOOP_COUNT and number of loops.
void RecordDownloadWriteLoopCount(int count);

// Record the number of buffers piled up by the IO thread
// before the file thread gets to draining them.
void RecordFileThreadReceiveBuffers(size_t num_buffers);

// Record the bandwidth seen in DownloadResourceHandler
// |actual_bandwidth| and |potential_bandwidth| are in bytes/second.
void RecordBandwidth(double actual_bandwidth, double potential_bandwidth);

// Record the time of both the first open and all subsequent opens since the
// download completed.
void RecordOpen(const base::Time& end, bool first);

// Record the number of items that are in the history at the time that a
// new download is added to the history.
void RecordHistorySize(int size);

// Record whether or not the server accepts ranges, and the download size .
void RecordAcceptsRanges(const std::string& accepts_ranges, int64 download_len);

// Record the total number of items and the number of in-progress items showing
// in the shelf when it closes.  Set |autoclose| to true when the shelf is
// closing itself, false when the user explicitly closed it.
CONTENT_EXPORT void RecordShelfClose(int size, int in_progress, bool autoclose);

// Record the number of downloads removed by ClearAll.
void RecordClearAllSize(int size);

// Record the number of completed unopened downloads when a download is opened.
void RecordOpensOutstanding(int size);

}  // namespace download_stats

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
