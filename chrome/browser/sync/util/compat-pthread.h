// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Pthread compatability routines.

#ifndef CHROME_BROWSER_SYNC_UTIL_COMPAT_PTHREAD_H_
#define CHROME_BROWSER_SYNC_UTIL_COMPAT_PTHREAD_H_

// TODO(timsteele): This file is deprecated. Use PlatformThread.
#include "base/platform_thread.h"

#define ThreadId PlatformThreadId

#ifndef OS_WINDOWS
inline ThreadId GetCurrentThreadId() {
  return PlatformThread::CurrentId();
}
#endif  // OS_WINDOWS

#if (!defined(OS_WINDOWS) && !defined(OS_MACOSX))
// TODO(timsteele): What the heck is this?
inline int sem_post_multiple(sem_t * sem, int number) {
  int i;
  int r = 0;
  for (i = 0; i < number; i++) {
    r = sem_post(sem);
    if (r != 0) {
      LOG_IF(ERROR, i > 0) << "sem_post() failed on iteration #" << i <<
        " of " << number;
      return r;
    }
  }
  return 0;
}
#endif  // (!defined(OS_WINDOWS) && !defined(OS_MACOSX))

#endif  // CHROME_BROWSER_SYNC_UTIL_COMPAT_PTHREAD_H_
