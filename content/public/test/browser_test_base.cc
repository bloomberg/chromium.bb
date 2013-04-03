// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_base.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/process_util.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/power_monitor/power_monitor.h"
#endif

#if defined(OS_ANDROID)
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/browser_thread.h"
#endif

namespace {

#if defined(OS_POSIX)
// On SIGTERM (sent by the runner on timeouts), dump a stack trace (to make
// debugging easier) and also exit with a known error code (so that the test
// framework considers this a failure -- http://crbug.com/57578).
// Note: We only want to do this in the browser process, and not forked
// processes. That might lead to hangs because of locks inside tcmalloc or the
// OS. See http://crbug.com/141302.
static int g_browser_process_pid;
static void DumpStackTraceSignalHandler(int signal) {
  if (g_browser_process_pid == base::GetCurrentProcId()) {
    logging::RawLog(logging::LOG_ERROR,
                    "BrowserTestBase signal handler received SIGTERM. "
                    "Backtrace:\n");
    base::debug::StackTrace().PrintBacktrace();
  }
  _exit(128 + signal);
}
#endif  // defined(OS_POSIX)

}  // namespace

namespace content {

extern int BrowserMain(const content::MainFunctionParams&);

BrowserTestBase::BrowserTestBase() {
#if defined(OS_MACOSX)
  base::mac::SetOverrideAmIBundled(true);
  base::PowerMonitor::AllocateSystemIOPorts();
#endif

#if defined(OS_POSIX)
  handle_sigterm_ = true;
#endif
}

BrowserTestBase::~BrowserTestBase() {
#if defined(OS_ANDROID)
  // RemoteTestServer can cause wait on the UI thread.
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  test_server_.reset(NULL);
#endif
}

void BrowserTestBase::SetUp() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  // The tests assume that file:// URIs can freely access other file:// URIs.
  command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);

  command_line->AppendSwitch(switches::kDomAutomationController);

  command_line->AppendSwitch(switches::kSkipGpuDataLoading);

  MainFunctionParams params(*command_line);
  params.ui_task =
      new base::Closure(
          base::Bind(&BrowserTestBase::ProxyRunTestOnMainThreadLoop, this));

  SetUpInProcessBrowserTestFixture();
#if defined(OS_ANDROID)
  BrowserMainRunner::Create()->Initialize(params);
  // We are done running the test by now. During teardown we
  // need to be able to perform IO.
  base::ThreadRestrictions::SetIOAllowed(true);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(base::IgnoreResult(&base::ThreadRestrictions::SetIOAllowed),
                 true));
#else
  BrowserMain(params);
#endif
  TearDownInProcessBrowserTestFixture();
}

void BrowserTestBase::TearDown() {
}

void BrowserTestBase::ProxyRunTestOnMainThreadLoop() {
#if defined(OS_POSIX)
  if (handle_sigterm_) {
    g_browser_process_pid = base::GetCurrentProcId();
    signal(SIGTERM, DumpStackTraceSignalHandler);
  }
#endif  // defined(OS_POSIX)
  RunTestOnMainThreadLoop();
}

void BrowserTestBase::CreateTestServer(const base::FilePath& test_server_base) {
  CHECK(!test_server_.get());
  test_server_.reset(new net::TestServer(
      net::TestServer::TYPE_HTTP,
      net::TestServer::kLocalhost,
      test_server_base));
}

}  // namespace content
