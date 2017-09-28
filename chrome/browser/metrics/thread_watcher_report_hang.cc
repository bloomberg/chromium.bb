// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/thread_watcher_report_hang.h"

// We disable optimizations for the whole file so the compiler doesn't merge
// them all together.
MSVC_DISABLE_OPTIMIZE()
MSVC_PUSH_DISABLE_WARNING(4748)

#include "base/debug/debugger.h"
#include "base/debug/dump_without_crashing.h"
#include "build/build_config.h"

namespace metrics {

// The following are unique function names for forcing the crash when a thread
// is unresponsive. This makes it possible to tell from the callstack alone what
// thread was unresponsive.
NOINLINE void ReportThreadHang() {
  volatile const char* inhibit_comdat = __func__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
#if defined(NDEBUG)
  base::debug::DumpWithoutCrashing();
#else
  base::debug::BreakDebugger();
#endif
}

#if !defined(OS_ANDROID) || !defined(NDEBUG)
NOINLINE void StartupHang() {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  // TODO(rtenneti): http://crbug.com/440885 enable crashing after fixing false
  // positive startup hang data.
  // ReportThreadHang();
}
#endif  // OS_ANDROID

NOINLINE void ShutdownHang() {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_UI() {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_DB() {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_FILE() {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_FILE_USER_BLOCKING() {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_PROCESS_LAUNCHER() {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_CACHE() {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_IO() {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  ReportThreadHang();
}

NOINLINE void CrashBecauseThreadWasUnresponsive(int thread_id) {
  // TODO(rtenneti): The following is a temporary change to check thread_id
  // numbers explicitly so that we will have minimum code. Will change after the
  // test run to use content::BrowserThread::ID enum.
  if (thread_id == 0)
    return ThreadUnresponsive_UI();
  else if (thread_id == 1)
    return ThreadUnresponsive_DB();
  else if (thread_id == 2)
    return ThreadUnresponsive_FILE();
  else if (thread_id == 3)
    return ThreadUnresponsive_FILE_USER_BLOCKING();
  else if (thread_id == 4)
    return ThreadUnresponsive_PROCESS_LAUNCHER();
  else if (thread_id == 5)
    return ThreadUnresponsive_CACHE();
  else if (thread_id == 6)
    return ThreadUnresponsive_IO();
}

}  // namespace metrics

MSVC_POP_WARNING()
MSVC_ENABLE_OPTIMIZE();
