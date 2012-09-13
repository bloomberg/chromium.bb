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
#include "base/system_monitor/system_monitor.h"
#endif

extern int BrowserMain(const content::MainFunctionParams&);

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
  if (g_browser_process_pid == base::GetCurrentProcId())
    base::debug::StackTrace().PrintBacktrace();
  _exit(128 + signal);
}
#endif  // defined(OS_POSIX)

}  // namespace

namespace content {

BrowserTestBase::BrowserTestBase() {
#if defined(OS_MACOSX)
  base::mac::SetOverrideAmIBundled(true);
  base::SystemMonitor::AllocateSystemIOPorts();
#endif

#if defined(OS_POSIX)
  handle_sigterm_ = true;
#endif
}

BrowserTestBase::~BrowserTestBase() {
}

void BrowserTestBase::SetUp() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  // The tests assume that file:// URIs can freely access other file:// URIs.
  command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);

  command_line->AppendSwitch(switches::kDomAutomationController);

  MainFunctionParams params(*command_line);
  params.ui_task =
      new base::Closure(
          base::Bind(&BrowserTestBase::ProxyRunTestOnMainThreadLoop, this));

  SetUpInProcessBrowserTestFixture();
  BrowserMain(params);
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

void BrowserTestBase::CreateTestServer(const char* test_server_base) {
  CHECK(!test_server_.get());
  test_server_.reset(new net::TestServer(
      net::TestServer::TYPE_HTTP,
      net::TestServer::kLocalhost,
      FilePath().AppendASCII(test_server_base)));
}

}  // namespace content
