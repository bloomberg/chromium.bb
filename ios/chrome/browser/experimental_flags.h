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

// Whether the app uses WKWebView instead of UIWebView.
bool IsWKWebViewEnabled();

// Returns the size in MB of the memory wedge to insert during a cold start.
// If 0, no memory wedge should be inserted.
size_t MemoryWedgeSizeInMB();

}  // namespace experimental_flags

#endif  // IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_
