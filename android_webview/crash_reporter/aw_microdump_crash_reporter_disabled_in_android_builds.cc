// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/crash_reporter/aw_microdump_crash_reporter.h"

// TODO(primiano): remove this once Android builds of WebView are deprecated.
// This translation unit is a no-op fallback for AwCrashReporter. This is built
// only when building in the Android tree.
// The rationale of this hack is to avoid the cost of maintaining breakpad in
// the Android tree, as the WebView build in Android itself is going to be
// deprecated soon (crbug.com/440792).

namespace android_webview {
namespace crash_reporter {

void EnableMicrodumpCrashReporter() {
}

}  // namespace crash_reporter
}  // namespace android_webview
