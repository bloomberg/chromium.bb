// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_stats.h"

#include "base/metrics/histogram.h"
#include "base/string_util.h"

namespace download_stats {

// All possible error codes from the network module. Note that the error codes
// are all positive (since histograms expect positive sample values).
const int kAllNetErrorCodes[] = {
#define NET_ERROR(label, value) -(value),
#include "net/base/net_error_list.h"
#undef NET_ERROR
};

void RecordDownloadCount(DownloadCountTypes type) {
  UMA_HISTOGRAM_ENUMERATION(
      "Download.Counts", type, DOWNLOAD_COUNT_TYPES_LAST_ENTRY);
}

void RecordDownloadCompleted(const base::TimeTicks& start, int64 download_len) {
  RecordDownloadCount(COMPLETED_COUNT);
  UMA_HISTOGRAM_LONG_TIMES("Download.Time", (base::TimeTicks::Now() - start));
  int64 max = 1024 * 1024 * 1024;  // One Terabyte.
  download_len /= 1024;  // In Kilobytes
  UMA_HISTOGRAM_CUSTOM_COUNTS("Download.DownloadSize",
                              download_len,
                              1,
                              max,
                              256);
}

void RecordDownloadInterrupted(int error, int64 received, int64 total) {
  RecordDownloadCount(INTERRUPTED_COUNT);
  UMA_HISTOGRAM_CUSTOM_ENUMERATION(
      "Download.InterruptedError",
      -error,
      base::CustomHistogram::ArrayToCustomRanges(
          kAllNetErrorCodes, arraysize(kAllNetErrorCodes)));

  // The maximum should be 2^kBuckets, to have the logarithmic bucket
  // boundaries fall on powers of 2.
  static const int kBuckets = 30;
  static const int64 kMaxKb = 1 << kBuckets;  // One Terabyte, in Kilobytes.
  int64 delta_bytes = total - received;
  bool unknown_size = total <= 0;
  int64 received_kb = received / 1024;
  int64 total_kb = total / 1024;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Download.InterruptedReceivedSizeK",
                              received_kb,
                              1,
                              kMaxKb,
                              kBuckets);
  if (!unknown_size) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Download.InterruptedTotalSizeK",
                                total_kb,
                                1,
                                kMaxKb,
                                kBuckets);
    if (delta_bytes == 0) {
      RecordDownloadCount(INTERRUPTED_AT_END_COUNT);
      UMA_HISTOGRAM_CUSTOM_ENUMERATION(
          "Download.InterruptedAtEndError",
          -error,
          base::CustomHistogram::ArrayToCustomRanges(
              kAllNetErrorCodes, arraysize(kAllNetErrorCodes)));
    } else if (delta_bytes > 0) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Download.InterruptedOverrunBytes",
                                  delta_bytes,
                                  1,
                                  kMaxKb,
                                  kBuckets);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Download.InterruptedUnderrunBytes",
                                  -delta_bytes,
                                  1,
                                  kMaxKb,
                                  kBuckets);
    }
  }

  UMA_HISTOGRAM_BOOLEAN("Download.InterruptedUnknownSize", unknown_size);
}

void RecordDownloadWriteSize(size_t data_len) {
  RecordDownloadCount(WRITE_SIZE_COUNT);
  int max = 1024 * 1024;  // One Megabyte.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Download.WriteSize", data_len, 1, max, 256);
}

void RecordDownloadWriteLoopCount(int count) {
  RecordDownloadCount(WRITE_LOOP_COUNT);
  UMA_HISTOGRAM_ENUMERATION("Download.WriteLoopCount", count, 20);
}

namespace {

enum DownloadContent {
  DOWNLOAD_CONTENT_UNRECOGNIZED = 0,
  DOWNLOAD_CONTENT_TEXT = 1,
  DOWNLOAD_CONTENT_IMAGE = 2,
  DOWNLOAD_CONTENT_AUDIO = 3,
  DOWNLOAD_CONTENT_VIDEO = 4,
  DOWNLOAD_CONTENT_OCTET_STREAM = 5,
  DOWNLOAD_CONTENT_PDF = 6,
  DOWNLOAD_CONTENT_DOC = 7,
  DOWNLOAD_CONTENT_XLS = 8,
  DOWNLOAD_CONTENT_PPT = 9,
  DOWNLOAD_CONTENT_ARCHIVE = 10,
  DOWNLOAD_CONTENT_EXE = 11,
  DOWNLOAD_CONTENT_DMG = 12,
  DOWNLOAD_CONTENT_CRX = 13,
  DOWNLOAD_CONTENT_MAX = 14,
};

struct MimeTypeToDownloadContent {
  const char* mime_type;
  DownloadContent download_content;
};

static MimeTypeToDownloadContent kMapMimeTypeToDownloadContent[] = {
  {"application/octet-stream", DOWNLOAD_CONTENT_OCTET_STREAM},
  {"binary/octet-stream", DOWNLOAD_CONTENT_OCTET_STREAM},
  {"application/pdf", DOWNLOAD_CONTENT_PDF},
  {"application/msword", DOWNLOAD_CONTENT_DOC},
  {"application/vnd.ms-excel", DOWNLOAD_CONTENT_XLS},
  {"application/vns.ms-powerpoint", DOWNLOAD_CONTENT_PPT},
  {"application/zip", DOWNLOAD_CONTENT_ARCHIVE},
  {"application/x-gzip", DOWNLOAD_CONTENT_ARCHIVE},
  {"application/x-rar-compressed", DOWNLOAD_CONTENT_ARCHIVE},
  {"application/x-tar", DOWNLOAD_CONTENT_ARCHIVE},
  {"application/x-bzip", DOWNLOAD_CONTENT_ARCHIVE},
  {"application/x-exe", DOWNLOAD_CONTENT_EXE},
  {"application/x-apple-diskimage", DOWNLOAD_CONTENT_DMG},
  {"application/x-chrome-extension", DOWNLOAD_CONTENT_CRX},
};

}  // namespace

void RecordDownloadMimeType(const std::string& mime_type_string) {
  DownloadContent download_content = DOWNLOAD_CONTENT_UNRECOGNIZED;

  // Look up exact matches.
  for (size_t i = 0; i < arraysize(kMapMimeTypeToDownloadContent); ++i) {
    const MimeTypeToDownloadContent& entry =
        kMapMimeTypeToDownloadContent[i];
    if (mime_type_string == entry.mime_type) {
      download_content = entry.download_content;
      break;
    }
  }

  // Do partial matches.
  if (download_content == DOWNLOAD_CONTENT_UNRECOGNIZED) {
    if (StartsWithASCII(mime_type_string, "text/", true)) {
      download_content = DOWNLOAD_CONTENT_TEXT;
    } else if (StartsWithASCII(mime_type_string, "image/", true)) {
      download_content = DOWNLOAD_CONTENT_IMAGE;
    } else if (StartsWithASCII(mime_type_string, "audio/", true)) {
      download_content = DOWNLOAD_CONTENT_AUDIO;
    } else if (StartsWithASCII(mime_type_string, "video/", true)) {
      download_content = DOWNLOAD_CONTENT_VIDEO;
    }
  }

  // Record the value.
  UMA_HISTOGRAM_ENUMERATION("Download.ContentType",
                            download_content,
                            DOWNLOAD_CONTENT_MAX);
}

}  // namespace download_stats
