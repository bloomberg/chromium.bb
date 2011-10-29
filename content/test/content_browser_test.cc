// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test.h"

#include "base/debug/stack_trace.h"
#include "base/message_loop.h"
#include "content/shell/shell.h"
#include "content/shell/shell_main_delegate.h"
#include "content/test/test_content_client.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

ContentBrowserTest::ContentBrowserTest() {
}

ContentBrowserTest::~ContentBrowserTest() {
}

void ContentBrowserTest::SetUp() {
  DCHECK(!content::GetContentClient());
  shell_main_delegate_.reset(new ShellMainDelegate);
  shell_main_delegate_->PreSandboxStartup();

  BrowserTestBase::SetUp();
}

void ContentBrowserTest::TearDown() {
  BrowserTestBase::TearDown();

  shell_main_delegate_.reset();
}

#if defined(OS_POSIX)
// On SIGTERM (sent by the runner on timeouts), dump a stack trace (to make
// debugging easier) and also exit with a known error code (so that the test
// framework considers this a failure -- http://crbug.com/57578).
static void DumpStackTraceSignalHandler(int signal) {
  base::debug::StackTrace().PrintBacktrace();
  _exit(128 + signal);
}
#endif  // defined(OS_POSIX)

void ContentBrowserTest::RunTestOnMainThreadLoop() {
#if defined(OS_POSIX)
  signal(SIGTERM, DumpStackTraceSignalHandler);
#endif  // defined(OS_POSIX)

#if defined(OS_MACOSX)
  // On Mac, without the following autorelease pool, code which is directly
  // executed (as opposed to executed inside a message loop) would autorelease
  // objects into a higher-level pool. This pool is not recycled in-sync with
  // the message loops' pools and causes problems with code relying on
  // deallocation via an autorelease pool (such as browser window closure and
  // browser shutdown). To avoid this, the following pool is recycled after each
  // time code is directly executed.
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  // Pump startup related events.
  MessageLoopForUI::current()->RunAllPending();

#if defined(OS_MACOSX)
  pool.Recycle();
#endif

  RunTestOnMainThread();
#if defined(OS_MACOSX)
  pool.Recycle();
#endif

  MessageLoopForUI::current()->Quit();
#if defined(OS_MACOSX)
  pool.Recycle();
#endif
}
