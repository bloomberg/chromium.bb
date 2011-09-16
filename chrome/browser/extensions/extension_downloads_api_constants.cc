// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_downloads_api_constants.h"

namespace extension_downloads_api_constants {

// Error messages
const char kNotImplemented[] = "NotImplemented";
const char kGenericError[] = "I'm afraid I can't do that.";
const char kInvalidURL[] = "Invalid URL";

// Parameter keys
const char kBodyKey[] = "body";
const char kBytesReceivedKey[] = "bytesReceived";
const char kDangerAcceptedKey[] = "dangerAccepted";
const char kDangerFile[] = "file";
const char kDangerKey[] = "danger";
const char kDangerSafe[] = "safe";
const char kDangerUrl[] = "url";
const char kEndTimeKey[] = "endTime";
const char kErrorKey[] = "error";
const char kFileSizeKey[] = "fileSize";
const char kFilenameKey[] = "filename";
const char kHeaderNameKey[] = "name";
const char kHeaderValueKey[] = "value";
const char kHeadersKey[] = "headers";
const char kIdKey[] = "id";
const char kMethodKey[] = "method";
const char kMimeKey[] = "mime";
const char kPausedKey[] = "paused";
const char kSaveAsKey[] = "saveAs";
const char kStartTimeKey[] = "startTime";
const char kStateComplete[] = "complete";
const char kStateInProgress[] = "in_progress";
const char kStateInterrupted[] = "interrupted";
const char kStateKey[] = "state";
const char kTotalBytesKey[] = "totalBytes";
const char kUrlKey[] = "url";

const char* DangerString(DownloadItem::DangerType danger) {
  switch (danger) {
    case DownloadItem::NOT_DANGEROUS: return kDangerSafe;
    case DownloadItem::DANGEROUS_FILE: return kDangerFile;
    case DownloadItem::DANGEROUS_URL: return kDangerUrl;
    default:
      NOTREACHED();
      return "";
  }
}

const char* StateString(DownloadItem::DownloadState state) {
  switch (state) {
    case DownloadItem::IN_PROGRESS: return kStateInProgress;
    case DownloadItem::COMPLETE: return kStateComplete;
    case DownloadItem::INTERRUPTED:  // fall through
    case DownloadItem::CANCELLED: return kStateInterrupted;
    case DownloadItem::REMOVING:  // fall through
    default:
      NOTREACHED();
      return "";
  }
}
}  // namespace extension_downloads_api_constants
