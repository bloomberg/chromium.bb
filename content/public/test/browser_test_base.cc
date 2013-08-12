// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_base.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_POSIX)
#include "base/process/process_handle.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/power_monitor/power_monitor_device_source.h"
#endif

#if defined(OS_ANDROID)
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/browser_thread.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

namespace content {
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

void RunTaskOnRendererThread(const base::Closure& task,
                             const base::Closure& quit_task) {
  task.Run();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_task);
}

}  // namespace

extern int BrowserMain(const MainFunctionParams&);

BrowserTestBase::BrowserTestBase()
    : embedded_test_server_(
        new net::test_server::EmbeddedTestServer(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO))),
      allow_test_contexts_(true),
      allow_osmesa_(true) {
#if defined(OS_MACOSX)
  base::mac::SetOverrideAmIBundled(true);
  base::PowerMonitorDeviceSource::AllocateSystemIOPorts();
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

#if defined(USE_AURA)
  // Use test contexts for browser tests unless they override and force us to
  // use a real context.
  if (allow_test_contexts_)
    command_line->AppendSwitch(switches::kTestCompositor);
#endif

  // When using real GL contexts, we usually use OSMesa as this works on all
  // bots. The command line can override this behaviour to use a real GPU.
  if (command_line->HasSwitch(switches::kUseGpuInTests))
    allow_osmesa_ = false;

  // Some bots pass this flag when they want to use a real GPU.
  if (command_line->HasSwitch("enable-gpu"))
    allow_osmesa_ = false;

#if defined(OS_MACOSX)
  // On Mac we always use a real GPU.
  allow_osmesa_ = false;
#endif

#if defined(OS_ANDROID)
  // On Android we always use a real GPU.
  allow_osmesa_ = false;
#endif

#if defined(OS_CHROMEOS)
  // If the test is running on the chromeos envrionment (such as
  // device or vm bots), the compositor will use real GL contexts, and
  // we should use real GL bindings with it.
  if (base::chromeos::IsRunningOnChromeOS())
    allow_osmesa_ = false;
#endif

  if (command_line->HasSwitch(switches::kUseGL)) {
    NOTREACHED() <<
        "kUseGL should not be used with tests. Try kUseGpuInTests instead.";
  }

  if (allow_osmesa_) {
    command_line->AppendSwitchASCII(
        switches::kUseGL, gfx::kGLImplementationOSMesaName);
  }

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
  embedded_test_server_.reset();
}

void BrowserTestBase::CreateTestServer(const base::FilePath& test_server_base) {
  CHECK(!test_server_.get());
  test_server_.reset(new net::SpawnedTestServer(
      net::SpawnedTestServer::TYPE_HTTP,
      net::SpawnedTestServer::kLocalhost,
      test_server_base));
}

void BrowserTestBase::PostTaskToInProcessRendererAndWait(
    const base::Closure& task) {
  CHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));

  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;

  base::MessageLoop* renderer_loop =
      RenderProcessHostImpl::GetInProcessRendererThreadForTesting();
  CHECK(renderer_loop);

  renderer_loop->PostTask(
      FROM_HERE,
      base::Bind(&RunTaskOnRendererThread, task, runner->QuitClosure()));
  runner->Run();
}

}  // namespace content
