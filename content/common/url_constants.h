// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.

#ifndef CONTENT_COMMON_URL_CONSTANTS_H_
#define CONTENT_COMMON_URL_CONSTANTS_H_
#pragma once

#include "content/common/content_export.h"

namespace chrome {

// Null terminated list of schemes that are savable.
extern const char* kSavableSchemes[];

// Canonical schemes you can use as input to GURL.SchemeIs().
// TODO(jam): some of these don't below in the content layer, but are accessed
// from there.
CONTENT_EXPORT extern const char kAboutScheme[];
CONTENT_EXPORT extern const char kBlobScheme[];
CONTENT_EXPORT extern const char kChromeDevToolsScheme[];
CONTENT_EXPORT extern const char kChromeInternalScheme[];
CONTENT_EXPORT extern const char kChromeUIScheme[];  // Used for WebUIs.
CONTENT_EXPORT extern const char kCrosScheme[];      // Used for ChromeOS.
CONTENT_EXPORT extern const char kDataScheme[];
CONTENT_EXPORT extern const char kExtensionScheme[];
CONTENT_EXPORT extern const char kFileScheme[];
CONTENT_EXPORT extern const char kFileSystemScheme[];
CONTENT_EXPORT extern const char kFtpScheme[];
CONTENT_EXPORT extern const char kHttpScheme[];
CONTENT_EXPORT extern const char kHttpsScheme[];
CONTENT_EXPORT extern const char kJavaScriptScheme[];
CONTENT_EXPORT extern const char kMailToScheme[];
CONTENT_EXPORT extern const char kMetadataScheme[];
CONTENT_EXPORT extern const char kViewSourceScheme[];

// Used to separate a standard scheme and the hostname: "://".
CONTENT_EXPORT extern const char kStandardSchemeSeparator[];

// About URLs (including schemes).
CONTENT_EXPORT extern const char kAboutBlankURL[];
extern const char kAboutCrashURL[];

// Special URL used to start a navigation to an error page.
extern const char kUnreachableWebDataURL[];

}  // namespace chrome

#endif  // CONTENT_COMMON_URL_CONSTANTS_H_
