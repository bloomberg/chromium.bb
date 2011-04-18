// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.

#ifndef CONTENT_COMMON_URL_CONSTANTS_H_
#define CONTENT_COMMON_URL_CONSTANTS_H_
#pragma once

#include "content/common/url_constants.h"

namespace chrome {

// Canonical schemes you can use as input to GURL.SchemeIs().
// TODO(jam): some of these don't below in the content layer, but are accessed
// from there.
extern const char kAboutScheme[];
extern const char kBlobScheme[];
extern const char kChromeDevToolsScheme[];
extern const char kChromeInternalScheme[];
extern const char kChromeUIScheme[];  // The scheme used for WebUIs.
extern const char kCrosScheme[];      // The scheme used for ChromeOS.
extern const char kDataScheme[];
extern const char kExtensionScheme[];
extern const char kFileScheme[];
extern const char kFileSystemScheme[];
extern const char kFtpScheme[];
extern const char kHttpScheme[];
extern const char kHttpsScheme[];
extern const char kJavaScriptScheme[];
extern const char kMailToScheme[];
extern const char kMetadataScheme[];
extern const char kUserScriptScheme[];
extern const char kViewSourceScheme[];

// Used to separate a standard scheme and the hostname: "://".
extern const char kStandardSchemeSeparator[];

// About URLs (including schemes).
extern const char kAboutBlankURL[];
extern const char kAboutCrashURL[];

// Special URL used to start a navigation to an error page.
extern const char kUnreachableWebDataURL[];

}  // namespace chrome

#endif  // CONTENT_COMMON_URL_CONSTANTS_H_
