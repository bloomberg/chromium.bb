// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_
#define IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_

#include <stdlib.h>

// This file can be empty. Its purpose is to contain the relatively short lived
// declarations required for experimental flags.

namespace experimental_flags {

// Whether background crash report upload should generate a local notification.
bool IsAlertOnBackgroundUploadEnabled();

// Whether external URL request blocking from subframes is enabled.
bool IsExternalURLBlockingEnabled();

// Whether the new bookmark collection experience is enabled.
bool IsBookmarkCollectionEnabled();

// Whether to extract salient images from pages at load time if bookmarked.
bool IsBookmarkImageFetchingOnVisitEnabled();

// Whether the app uses WKWebView instead of UIWebView.
bool IsWKWebViewEnabled();

// Whether viewing and copying passwords is enabled.
bool IsViewCopyPasswordsEnabled();

// Returns the size in MB of the memory wedge to insert during a cold start.
// If 0, no memory wedge should be inserted.
size_t MemoryWedgeSizeInMB();

}  // namespace experimental_flags

#endif  // IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_
