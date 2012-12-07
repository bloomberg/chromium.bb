// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_CHROME_GPU_UTIL_H_
#define CHROME_BROWSER_GPU_CHROME_GPU_UTIL_H_

namespace gpu_util {

// Sets up a monitor for browser windows, to be used to determine gpu
// managed memory allocation.
// Not supported on Android.
#if !defined(OS_ANDROID)
void InstallBrowserMonitor();
void UninstallBrowserMonitor();
#endif // !defined(OS_ANDROID)

// Sets up force-compositing-mode and threaded compositing field trials.
void InitializeCompositingFieldTrial();

}  // namespace gpu_util

#endif  // CHROME_BROWSER_GPU_CHROME_GPU_UTIL_H_

