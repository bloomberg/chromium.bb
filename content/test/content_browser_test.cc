// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test.h"

#include "base/debug/stack_trace.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop.h"
#include "content/test/test_content_client.h"

ContentBrowserTest::ContentBrowserTest() {
}

ContentBrowserTest::~ContentBrowserTest() {
}

void ContentBrowserTest::SetUp() {
  DCHECK(!content::GetContentClient());
  content_client_.reset(new TestContentClient);
  content::SetContentClient(content_client_.get());

  content_browser_client_.reset(new content::MockContentBrowserClient());
  content_client_->set_browser(content_browser_client_.get());

  BrowserTestBase::SetUp();
}

void ContentBrowserTest::TearDown() {
  BrowserTestBase::TearDown();

  DCHECK_EQ(content_client_.get(), content::GetContentClient());
  content::SetContentClient(NULL);
  content_client_.reset();

  content_browser_client_.reset();
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

  // On Mac, without the following autorelease pool, code which is directly
  // executed (as opposed to executed inside a message loop) would autorelease
  // objects into a higher-level pool. This pool is not recycled in-sync with
  // the message loops' pools and causes problems with code relying on
  // deallocation via an autorelease pool (such as browser window closure and
  // browser shutdown). To avoid this, the following pool is recycled after each
  // time code is directly executed.
  base::mac::ScopedNSAutoreleasePool pool;

  // Pump startup related events.
  MessageLoopForUI::current()->RunAllPending();
  pool.Recycle();

  RunTestOnMainThread();
  pool.Recycle();

  MessageLoopForUI::current()->Quit();
  pool.Recycle();
}