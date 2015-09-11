// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_ANDROID_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_ANDROID_H_

#include <string>

class GURL;

namespace content {

// Initialize the nested message loop and FIFOs for Android, and verify that
// all has been set up using a few appropriate CHECK()s.
void EnsureInitializeForAndroidLayoutTests();

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_ANDROID_H_
