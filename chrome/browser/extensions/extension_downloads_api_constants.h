// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DOWNLOADS_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DOWNLOADS_API_CONSTANTS_H_
#pragma once

#include "content/browser/download/download_item.h"

namespace extension_downloads_api_constants {

// Error messages
extern const char kNotImplemented[];
extern const char kGenericError[];
extern const char kInvalidURL[];

// Parameter keys
extern const char kBodyKey[];
extern const char kBytesReceivedKey[];
extern const char kDangerAcceptedKey[];
extern const char kDangerFile[];
extern const char kDangerKey[];
extern const char kDangerSafe[];
extern const char kDangerUrl[];
extern const char kEndTimeKey[];
extern const char kErrorKey[];
extern const char kFileSizeKey[];
extern const char kFilenameKey[];
extern const char kHeaderNameKey[];
extern const char kHeaderValueKey[];
extern const char kHeadersKey[];
extern const char kIdKey[];
extern const char kMethodKey[];
extern const char kMimeKey[];
extern const char kPausedKey[];
extern const char kSaveAsKey[];
extern const char kStartTimeKey[];
extern const char kStateComplete[];
extern const char kStateInProgress[];
extern const char kStateInterrupted[];
extern const char kStateKey[];
extern const char kTotalBytesKey[];
extern const char kUrlKey[];

const char* DangerString(DownloadItem::DangerType danger);

const char* StateString(DownloadItem::DownloadState state);

}  // namespace extension_downloads_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DOWNLOADS_API_CONSTANTS_H_
