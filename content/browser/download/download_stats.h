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

namespace base {
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

  DOWNLOAD_COUNT_TYPES_LAST_ENTRY
};

// Increment one of the above counts.
void RecordDownloadCount(DownloadCountTypes type);

// Record COMPLETED_COUNT and how long the download took.
void RecordDownloadCompleted(const base::TimeTicks& start, int64 download_len);

// Record INTERRUPTED_COUNT, |error|, |received| and |total| bytes.
void RecordDownloadInterrupted(int error, int64 received, int64 total);

// Records the mime type of the download.
void RecordDownloadMimeType(const std::string& mime_type);

// Record WRITE_SIZE_COUNT and data_len.
void RecordDownloadWriteSize(size_t data_len);

// Record WRITE_LOOP_COUNT and number of loops.
void RecordDownloadWriteLoopCount(int count);

}  // namespace download_stats

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
