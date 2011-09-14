// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DOWNLOADS_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DOWNLOADS_API_CONSTANTS_H_
#pragma once

#include "content/browser/download/download_item.h"

namespace extension_downloads_api_constants {

extern const char kNotImplemented[];
extern const char kGenericError[];
extern const char kUrlKey[];
extern const char kFilenameKey[];
extern const char kSaveAsKey[];
extern const char kMethodKey[];
extern const char kHeadersKey[];
extern const char kBodyKey[];
extern const char kDangerSafe[];
extern const char kDangerFile[];
extern const char kDangerUrl[];
extern const char kStateInProgress[];
extern const char kStateComplete[];
extern const char kStateInterrupted[];
extern const char kIdKey[];
extern const char kDangerKey[];
extern const char kDangerAcceptedKey[];
extern const char kStateKey[];
extern const char kPausedKey[];
extern const char kMimeKey[];
extern const char kStartTimeKey[];
extern const char kEndTimeKey[];
extern const char kBytesReceivedKey[];
extern const char kTotalBytesKey[];
extern const char kFileSizeKey[];
extern const char kErrorKey[];

const char* DangerString(DownloadItem::DangerType danger);

const char* StateString(DownloadItem::DownloadState state);

}  // namespace extension_downloads_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DOWNLOADS_API_CONSTANTS_H_
