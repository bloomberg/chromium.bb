// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_LAYOUT_TESTS_ANDROID_H_
#define CONTENT_SHELL_BROWSER_SHELL_LAYOUT_TESTS_ANDROID_H_

#include <string>

class GURL;

namespace content {

// On Android, all passed tests will be paths to a local temporary directory.
// However, because we can't transfer all test files to the device, translate
// those paths to a local, forwarded URL so the host can serve them.
bool GetTestUrlForAndroid(std::string& path_or_url, GURL* url);

// Initialize the nested message loop and FIFOs for Android, and verify that
// all has been set up using a few appropriate CHECK()s.
void EnsureInitializeForAndroidLayoutTests();

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_LAYOUT_TESTS_ANDROID_H_
