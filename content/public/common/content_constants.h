// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the Content application.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_CONSTANTS_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_CONSTANTS_H_
#pragma once

#include <stddef.h>         // For size_t

#include "base/file_path.h"
#include "content/common/content_export.h"

namespace content {

// The name of the directory under BrowserContext::GetPath where the AppCache is
// put.
CONTENT_EXPORT extern const FilePath::CharType kAppCacheDirname[];
// The name of the directory under BrowserContext::GetPath where Pepper plugin
// data is put.
CONTENT_EXPORT extern const FilePath::CharType kPepperDataDirname[];

// The MIME type used for the browser plugin.
CONTENT_EXPORT extern const char kBrowserPluginMimeType[];

CONTENT_EXPORT extern const size_t kMaxRendererProcessCount;

// The maximum number of session history entries per tab.
extern const int kMaxSessionHistoryEntries;

// The maximum number of characters of the document's title that we're willing
// to accept in the browser process.
extern const size_t kMaxTitleChars;

// The maximum number of characters in the URL that we're willing to accept
// in the browser process. It is set low enough to avoid damage to the browser
// but high enough that a web site can abuse location.hash for a little storage.
// We have different values for "max accepted" and "max displayed" because
// a data: URI may be legitimately massive, but the full URI would kill all
// known operating systems if you dropped it into a UI control.
CONTENT_EXPORT extern const size_t kMaxURLChars;
CONTENT_EXPORT extern const size_t kMaxURLDisplayChars;

extern const char kStatsFilename[];
extern const int kStatsMaxThreads;
extern const int kStatsMaxCounters;

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_CONSTANTS_H_
