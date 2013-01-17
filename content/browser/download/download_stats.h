// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Holds helpers for gathering UMA stats about downloads.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_

#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_interrupt_reasons.h"

namespace base {
class Time;
class TimeDelta;
class TimeTicks;
}

namespace content {

// We keep a count of how often various events occur in the
// histogram "Download.Counts".
enum DownloadCountTypes {
  // Stale enum values left around so that values passed to UMA don't
  // change.
  DOWNLOAD_COUNT_UNUSED_0 = 0,
  DOWNLOAD_COUNT_UNUSED_1,
  DOWNLOAD_COUNT_UNUSED_2,
  DOWNLOAD_COUNT_UNUSED_3,
  DOWNLOAD_COUNT_UNUSED_4,

  // Downloads that made it to DownloadResourceHandler
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

  // Counts errors due to writes to BaseFiles that have been detached already.
  // This can happen when saving web pages as complete packages. It happens
  // when we get messages to append data to files that have already finished and
  // been detached, but haven't yet been removed from the list of files in
  // progress.
  APPEND_TO_DETACHED_FILE_COUNT,

  // Counts the number of instances where the downloaded file is missing after a
  // successful invocation of ScanAndSaveDownloadedFile().
  FILE_MISSING_AFTER_SUCCESSFUL_SCAN_COUNT,

  DOWNLOAD_COUNT_TYPES_LAST_ENTRY
};

enum DownloadSource {
  // The download was initiated when the SavePackage system rejected
  // a Save Page As ... by returning false from
  // SavePackage::IsSaveableContents().
  INITIATED_BY_SAVE_PACKAGE_ON_NON_HTML = 0,

  // The download was initiated by a drag and drop from a drag-and-drop
  // enabled web application.
  INITIATED_BY_DRAG_N_DROP,

  // The download was initiated by explicit RPC from the renderer process
  // (e.g. by Alt-click) through the IPC ViewHostMsg_DownloadUrl.
  INITIATED_BY_RENDERER,

  // The download was initiated by a renderer or plugin process through
  // the IPC ViewHostMsg_SaveURLAs; currently this is only used by the
  // Pepper plugin API.
  INITIATED_BY_PEPPER_SAVE,

  DOWNLOAD_SOURCE_LAST_ENTRY
};

// Increment one of the above counts.
void RecordDownloadCount(DownloadCountTypes type);

// Record initiation of a download from a specific source.
void RecordDownloadSource(DownloadSource source);

// Record COMPLETED_COUNT and how long the download took.
void RecordDownloadCompleted(const base::TimeTicks& start, int64 download_len);

// Record INTERRUPTED_COUNT, |reason|, |received| and |total| bytes.
void RecordDownloadInterrupted(DownloadInterruptReason reason,
                               int64 received,
                               int64 total);

// Records the mime type of the download.
void RecordDownloadMimeType(const std::string& mime_type);

// Records usage of Content-Disposition header.
void RecordDownloadContentDisposition(const std::string& content_disposition);

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

// Record whether or not the server accepts ranges, and the download size.
void RecordAcceptsRanges(const std::string& accepts_ranges, int64 download_len);

// Record the number of downloads removed by ClearAll.
void RecordClearAllSize(int size);

// Record the number of completed unopened downloads when a download is opened.
void RecordOpensOutstanding(int size);

// Record how long we block the file thread at a time.
void RecordContiguousWriteTime(base::TimeDelta time_blocked);

// Record the percentage of time we had to block the network (i.e.
// how often, for each download, something other than the network
// was the bottleneck).
void RecordNetworkBlockage(base::TimeDelta resource_handler_lifetime,
                           base::TimeDelta resource_handler_blocked_time);

// Record overall bandwidth stats at the file end.
void RecordFileBandwidth(size_t length,
                         base::TimeDelta disk_write_time,
                         base::TimeDelta elapsed_time);

enum SavePackageEvent {
  // The user has started to save a page as a package.
  SAVE_PACKAGE_STARTED,

  // The save package operation was cancelled.
  SAVE_PACKAGE_CANCELLED,

  // The save package operation finished without being cancelled.
  SAVE_PACKAGE_FINISHED,

  // The save package tried to write to an already completed file.
  SAVE_PACKAGE_WRITE_TO_COMPLETED,

  // The save package tried to write to an already failed file.
  SAVE_PACKAGE_WRITE_TO_FAILED,

  SAVE_PACKAGE_LAST_ENTRY
};

void RecordSavePackageEvent(SavePackageEvent event);

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_STATS_H_
