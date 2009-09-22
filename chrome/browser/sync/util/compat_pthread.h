// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Pthread compatability routines.
// TODO(timsteele): This file is deprecated. Use PlatformThread.

#ifndef CHROME_BROWSER_SYNC_UTIL_COMPAT_PTHREAD_H_
#define CHROME_BROWSER_SYNC_UTIL_COMPAT_PTHREAD_H_

#include "base/platform_thread.h"
#include "build/build_config.h"

#define ThreadId PlatformThreadId

#ifndef OS_WIN
inline ThreadId GetCurrentThreadId() {
  return PlatformThread::CurrentId();
}
#endif  // OS_WIN

#endif  // CHROME_BROWSER_SYNC_UTIL_COMPAT_PTHREAD_H_
