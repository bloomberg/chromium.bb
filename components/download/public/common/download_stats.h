// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Holds helpers for gathering UMA stats about downloads.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_STATS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_STATS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "components/download/public/common/download_content.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_source.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class Time;
class TimeDelta;
class TimeTicks;
}  // namespace base

namespace download {

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

  // Downloads that are started.
  START_COUNT,

  // Downloads that were interrupted by the OS.
  INTERRUPTED_COUNT,

  // (Deprecated) Write sizes for downloads.
  // This is equal to the number of samples in Download.WriteSize histogram.
  DOWNLOAD_COUNT_UNUSED_10,

  // (Deprecated) Counts iterations of the BaseFile::AppendDataToFile() loop.
  // This is equal to the number of samples in Download.WriteLoopCount
  // histogram.
  DOWNLOAD_COUNT_UNUSED_11,

  // Counts interruptions that happened at the end of the download.
  INTERRUPTED_AT_END_COUNT,

  // Counts errors due to writes to BaseFiles that have been detached already.
  // This can happen when saving web pages as complete packages. It happens
  // when we get messages to append data to files that have already finished and
  // been detached, but haven't yet been removed from the list of files in
  // progress.
  APPEND_TO_DETACHED_FILE_COUNT,

  // (Deprecated) Counts the number of instances where the downloaded file is
  // missing after a successful invocation of ScanAndSaveDownloadedFile().
  DOWNLOAD_COUNT_UNUSED_14,

  // (Deprecated) Count of downloads with a strong ETag and specified
  // 'Accept-Ranges: bytes'.
  DOWNLOAD_COUNT_UNUSED_15,

  // Count of downloads that didn't have a valid WebContents at the time it was
  // interrupted.
  INTERRUPTED_WITHOUT_WEBCONTENTS,

  // Count of downloads that supplies a strong validator (implying byte-wise
  // equivalence) and has a 'Accept-Ranges: bytes' header. These downloads are
  // candidates for partial resumption.
  STRONG_VALIDATOR_AND_ACCEPTS_RANGES,

  // (Deprecated) Count of downloads that uses parallel download requests.
  USES_PARALLEL_REQUESTS,

  // Count of new downloads.
  NEW_DOWNLOAD_COUNT,

  // Count of new downloads that are started in normal profile.
  NEW_DOWNLOAD_COUNT_NORMAL_PROFILE,

  // Downloads that are actually completed in normal profile.
  COMPLETED_COUNT_NORMAL_PROFILE,

  // Downloads that are completed with a content length mismatch error.
  COMPLETED_WITH_CONTENT_LENGTH_MISMATCH_COUNT,

  // After a download is interrupted with a content length mismatch error, more
  // bytes are received when resuming the download.
  MORE_BYTES_RECEIVED_AFTER_CONTENT_LENGTH_MISMATCH_COUNT,

  // After a download is interrupted with a content length mismatch error, no
  // bytes are received when resuming the download.
  NO_BYTES_RECEIVED_AFTER_CONTENT_LENGTH_MISMATCH_COUNT,

  // Count of downloads that requested target determination.
  DETERMINE_DOWNLOAD_TARGET_COUNT,

  // Count of downloads that has target determination completed.
  DOWNLOAD_TARGET_DETERMINED_COUNT,

  // A cross origin download without a content disposition header.
  CROSS_ORIGIN_DOWNLOAD_WITHOUT_CONTENT_DISPOSITION,

  // Count of attempts that triggered the download flow, before any network
  // requests are sent.
  DOWNLOAD_TRIGGERED_COUNT,

  // Count of attempts for manual download resumption.
  MANUAL_RESUMPTION_COUNT,

  // Count of attempts for auto download resumption.
  AUTO_RESUMPTION_COUNT,

  // Count of download attempts that are dropped due to content settings or
  // request limiter before DownloadItem is created.
  DOWNLOAD_DROPPED_COUNT,

  DOWNLOAD_COUNT_TYPES_LAST_ENTRY
};

enum DownloadDiscardReason {
  // The download is being discarded due to a user action.
  DOWNLOAD_DISCARD_DUE_TO_USER_ACTION,

  // The download is being discarded due to the browser being shut down.
  DOWNLOAD_DISCARD_DUE_TO_SHUTDOWN
};

// When parallel download is enabled, the download may fall back to a normal
// download for various reasons. This enum counts the number of parallel
// download and fallbacks. Also records the reasons why the download falls back
// to a normal download. The reasons are not mutually exclusive.
// Used in histogram "Download.ParallelDownload.CreationEvent" and should be
// treated as append-only.
enum class ParallelDownloadCreationEvent {
  // The total number of downloads started as parallel download.
  STARTED_PARALLEL_DOWNLOAD = 0,

  // The total number of downloads fell back to normal download when parallel
  // download is enabled.
  FELL_BACK_TO_NORMAL_DOWNLOAD,

  // No ETag or Last-Modified response header.
  FALLBACK_REASON_STRONG_VALIDATORS,

  // No Accept-Range response header.
  FALLBACK_REASON_ACCEPT_RANGE_HEADER,

  // No Content-Length response header.
  FALLBACK_REASON_CONTENT_LENGTH_HEADER,

  // File size is not complied to finch configuration.
  FALLBACK_REASON_FILE_SIZE,

  // The HTTP connection type does not meet the requirement.
  FALLBACK_REASON_CONNECTION_TYPE,

  // The remaining time does not meet the requirement.
  FALLBACK_REASON_REMAINING_TIME,

  // The http method or url scheme does not meet the requirement.
  FALLBACK_REASON_HTTP_METHOD,

  // Last entry of the enum.
  COUNT,
};

// Increment one of the above counts.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadCount(DownloadCountTypes type);

// Record download count with download source.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadCountWithSource(
    DownloadCountTypes type,
    DownloadSource download_source);

// Record COMPLETED_COUNT and how long the download took.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadCompleted(
    const base::TimeTicks& start,
    int64_t download_len,
    bool is_parallelizable,
    DownloadSource download_source);

// Record download deletion event.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadDeletion(
    base::Time completion_time,
    const std::string& mime_type);

// Record INTERRUPTED_COUNT, |reason|, |received| and |total| bytes.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadInterrupted(
    DownloadInterruptReason reason,
    int64_t received,
    int64_t total,
    bool is_parallelizable,
    bool is_parallel_download_enabled,
    DownloadSource download_source);

// Record that a download has been classified as malicious.
COMPONENTS_DOWNLOAD_EXPORT void RecordMaliciousDownloadClassified(
    DownloadDangerType danger_type);

// Record a dangerous download accept event.
COMPONENTS_DOWNLOAD_EXPORT void RecordDangerousDownloadAccept(
    DownloadDangerType danger_type,
    const base::FilePath& file_path);

// Record a dangerous download discard event.
COMPONENTS_DOWNLOAD_EXPORT void RecordDangerousDownloadDiscard(
    DownloadDiscardReason reason,
    DownloadDangerType danger_type,
    const base::FilePath& file_path);

// Returns the type of download.
COMPONENTS_DOWNLOAD_EXPORT DownloadContent
DownloadContentFromMimeType(const std::string& mime_type_string,
                            bool record_content_subcategory);

// Records the mime type of the download.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadMimeType(
    const std::string& mime_type);

// Records the mime type of the download for normal profile.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadMimeTypeForNormalProfile(
    const std::string& mime_type);

// Records usage of Content-Disposition header.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadContentDisposition(
    const std::string& content_disposition);

// Record the number of buffers piled up by the IO thread
// before the file thread gets to draining them.
COMPONENTS_DOWNLOAD_EXPORT void RecordFileThreadReceiveBuffers(
    size_t num_buffers);

// Record the time of both the first open and all subsequent opens since the
// download completed.
COMPONENTS_DOWNLOAD_EXPORT void RecordOpen(const base::Time& end, bool first);

// Record whether or not the server accepts ranges, and the download size. Also
// counts if a strong validator is supplied. The combination of range request
// support and ETag indicates downloads that are candidates for partial
// resumption.
COMPONENTS_DOWNLOAD_EXPORT void RecordAcceptsRanges(
    const std::string& accepts_ranges,
    int64_t download_len,
    bool has_strong_validator);

// Record the number of completed unopened downloads when a download is opened.
COMPONENTS_DOWNLOAD_EXPORT void RecordOpensOutstanding(int size);

// Record how long we block the file thread at a time.
COMPONENTS_DOWNLOAD_EXPORT void RecordContiguousWriteTime(
    base::TimeDelta time_blocked);

// Record the percentage of time we had to block the network (i.e.
// how often, for each download, something other than the network
// was the bottleneck).
COMPONENTS_DOWNLOAD_EXPORT void RecordNetworkBlockage(
    base::TimeDelta resource_handler_lifetime,
    base::TimeDelta resource_handler_blocked_time);

// Record overall bandwidth stats at the file end.
// Does not count in any hash computation or file open/close time.
COMPONENTS_DOWNLOAD_EXPORT void RecordFileBandwidth(
    size_t length,
    base::TimeDelta disk_write_time,
    base::TimeDelta elapsed_time);

// Records the size of the download from content-length header.
COMPONENTS_DOWNLOAD_EXPORT void RecordParallelizableContentLength(
    int64_t content_length);

// Increment one of the count for parallelizable download.
COMPONENTS_DOWNLOAD_EXPORT void RecordParallelizableDownloadCount(
    DownloadCountTypes type,
    bool is_parallel_download_enabled);

// Records the actual total number of requests sent for a parallel download,
// including the initial request.
COMPONENTS_DOWNLOAD_EXPORT void RecordParallelDownloadRequestCount(
    int request_count);

// Records if each byte stream is successfully added to download sink.
COMPONENTS_DOWNLOAD_EXPORT void RecordParallelDownloadAddStreamSuccess(
    bool success);

// Records the bandwidth for parallelizable download and estimates the saved
// time at the file end. Does not count in any hash computation or file
// open/close time.
COMPONENTS_DOWNLOAD_EXPORT void RecordParallelizableDownloadStats(
    size_t bytes_downloaded_with_parallel_streams,
    base::TimeDelta time_with_parallel_streams,
    size_t bytes_downloaded_without_parallel_streams,
    base::TimeDelta time_without_parallel_streams,
    bool uses_parallel_requests);

// Records the average bandwidth, time, and file size for parallelizable
// download.
COMPONENTS_DOWNLOAD_EXPORT void RecordParallelizableDownloadAverageStats(
    int64_t bytes_downloaded,
    const base::TimeDelta& time_span);

// Records the parallel download creation counts and the reasons why the
// download falls back to non-parallel download.
COMPONENTS_DOWNLOAD_EXPORT void RecordParallelDownloadCreationEvent(
    ParallelDownloadCreationEvent event);

// Record the result of a download file rename.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadFileRenameResultAfterRetry(
    base::TimeDelta time_since_first_failure,
    DownloadInterruptReason interrupt_reason);

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

  // Instead of using save package API, used download API to save non HTML
  // format files.
  SAVE_PACKAGE_DOWNLOAD_ON_NON_HTML,

  SAVE_PACKAGE_LAST_ENTRY
};

COMPONENTS_DOWNLOAD_EXPORT void RecordSavePackageEvent(SavePackageEvent event);

enum OriginStateOnResumption {
  ORIGIN_STATE_ON_RESUMPTION_ADDITIONAL_REDIRECTS = 1 << 0,
  ORIGIN_STATE_ON_RESUMPTION_VALIDATORS_CHANGED = 1 << 1,
  ORIGIN_STATE_ON_RESUMPTION_CONTENT_DISPOSITION_CHANGED = 1 << 2,
  ORIGIN_STATE_ON_RESUMPTION_MAX = 1 << 3
};

// Record the state of the origin information across a download resumption
// request. |state| is a combination of values from OriginStateOnResumption
// enum.
COMPONENTS_DOWNLOAD_EXPORT void RecordOriginStateOnResumption(
    bool is_partial,
    OriginStateOnResumption state);

COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadConnectionSecurity(
    const GURL& download_url,
    const std::vector<GURL>& url_chain);

COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadContentTypeSecurity(
    const GURL& download_url,
    const std::vector<GURL>& url_chain,
    const std::string& mime_type,
    const base::RepeatingCallback<bool(const GURL&)>&
        is_origin_secure_callback);

COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadSourcePageTransitionType(
    const base::Optional<ui::PageTransition>& transition);

COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadHttpResponseCode(
    int response_code);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_STATS_H_
