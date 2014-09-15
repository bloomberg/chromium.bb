// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_base.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/test/test_timeouts.h"
#include "content/public/app/content_main.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_POSIX)
#include "base/process/process_handle.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_ANDROID)
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/browser_thread.h"
#endif

#if defined(USE_AURA)
#include "content/browser/compositor/image_transport_factory.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#if defined(USE_X11)
#include "ui/aura/window_tree_host_x11.h"
#endif
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
    base::debug::StackTrace().Print();
  }
  _exit(128 + signal);
}
#endif  // defined(OS_POSIX)

void RunTaskOnRendererThread(const base::Closure& task,
                             const base::Closure& quit_task) {
  task.Run();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_task);
}

// In many cases it may be not obvious that a test makes a real DNS lookup.
// We generally don't want to rely on external DNS servers for our tests,
// so this host resolver procedure catches external queries and returns a failed
// lookup result.
class LocalHostResolverProc : public net::HostResolverProc {
 public:
  LocalHostResolverProc() : HostResolverProc(NULL) {}

  virtual int Resolve(const std::string& host,
                      net::AddressFamily address_family,
                      net::HostResolverFlags host_resolver_flags,
                      net::AddressList* addrlist,
                      int* os_error) OVERRIDE {
    const char* kLocalHostNames[] = {"localhost", "127.0.0.1", "::1"};
    bool local = false;

    if (host == net::GetHostName()) {
      local = true;
    } else {
      for (size_t i = 0; i < arraysize(kLocalHostNames); i++)
        if (host == kLocalHostNames[i]) {
          local = true;
          break;
        }
    }

    // To avoid depending on external resources and to reduce (if not preclude)
    // network interactions from tests, we simulate failure for non-local DNS
    // queries, rather than perform them.
    // If you really need to make an external DNS query, use
    // net::RuleBasedHostResolverProc and its AllowDirectLookup method.
    if (!local) {
      DVLOG(1) << "To avoid external dependencies, simulating failure for "
          "external DNS lookup of " << host;
      return net::ERR_NOT_IMPLEMENTED;
    }

    return ResolveUsingPrevious(host, address_family, host_resolver_flags,
                                addrlist, os_error);
  }

 private:
  virtual ~LocalHostResolverProc() {}
};

void TraceDisableRecordingComplete(const base::Closure& quit,
                                   const base::FilePath& file_path) {
  LOG(ERROR) << "Tracing written to: " << file_path.value();
  quit.Run();
}

}  // namespace

extern int BrowserMain(const MainFunctionParams&);

BrowserTestBase::BrowserTestBase()
    : expected_exit_code_(0),
      enable_pixel_output_(false),
      use_software_compositing_(false) {
#if defined(OS_MACOSX)
  base::mac::SetOverrideAmIBundled(true);
#endif

#if defined(USE_AURA)
#if defined(USE_X11)
  aura::test::SetUseOverrideRedirectWindowByDefault(true);
#endif
  aura::test::InitializeAuraEventGeneratorDelegate();
#endif

#if defined(OS_POSIX)
  handle_sigterm_ = true;
#endif

  // This is called through base::TestSuite initially. It'll also be called
  // inside BrowserMain, so tell the code to ignore the check that it's being
  // called more than once
  base::i18n::AllowMultipleInitializeCallsForTesting();

  embedded_test_server_.reset(new net::test_server::EmbeddedTestServer);
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

  // Override the child process connection timeout since tests can exceed that
  // when sharded.
  command_line->AppendSwitchASCII(
      switches::kIPCConnectionTimeout,
      base::IntToString(TestTimeouts::action_max_timeout().InSeconds()));

  // The tests assume that file:// URIs can freely access other file:// URIs.
  command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);

  command_line->AppendSwitch(switches::kDomAutomationController);

  // It is sometimes useful when looking at browser test failures to know which
  // GPU blacklisting decisions were made.
  command_line->AppendSwitch(switches::kLogGpuControlListDecisions);

  if (use_software_compositing_) {
    command_line->AppendSwitch(switches::kDisableGpu);
#if defined(USE_AURA)
    command_line->AppendSwitch(switches::kUIDisableThreadedCompositing);
#endif
  }

#if defined(USE_AURA)
  // Most tests do not need pixel output, so we don't produce any. The command
  // line can override this behaviour to allow for visual debugging.
  if (command_line->HasSwitch(switches::kEnablePixelOutputInTests))
    enable_pixel_output_ = true;

  if (command_line->HasSwitch(switches::kDisableGLDrawingForTests)) {
    NOTREACHED() << "kDisableGLDrawingForTests should not be used as it"
                    "is chosen by tests. Use kEnablePixelOutputInTests "
                    "to enable pixel output.";
  }

  // Don't enable pixel output for browser tests unless they override and force
  // us to, or it's requested on the command line.
  if (!enable_pixel_output_ && !use_software_compositing_)
    command_line->AppendSwitch(switches::kDisableGLDrawingForTests);
#endif

  bool use_osmesa = true;

  // We usually use OSMesa as this works on all bots. The command line can
  // override this behaviour to use hardware GL.
  if (command_line->HasSwitch(switches::kUseGpuInTests))
    use_osmesa = false;

  // Some bots pass this flag when they want to use hardware GL.
  if (command_line->HasSwitch("enable-gpu"))
    use_osmesa = false;

#if defined(OS_MACOSX)
  // On Mac we always use hardware GL.
  use_osmesa = false;
#endif

#if defined(OS_ANDROID)
  // On Android we always use hardware GL.
  use_osmesa = false;
#endif

#if defined(OS_CHROMEOS)
  // If the test is running on the chromeos envrionment (such as
  // device or vm bots), we use hardware GL.
  if (base::SysInfo::IsRunningOnChromeOS())
    use_osmesa = false;
#endif

  if (use_osmesa && !use_software_compositing_)
    command_line->AppendSwitch(switches::kOverrideUseGLWithOSMesaForTests);

  scoped_refptr<net::HostResolverProc> local_resolver =
      new LocalHostResolverProc();
  rule_based_resolver_ =
      new net::RuleBasedHostResolverProc(local_resolver.get());
  rule_based_resolver_->AddSimulatedFailure("wpad");
  net::ScopedDefaultHostResolverProc scoped_local_host_resolver_proc(
      rule_based_resolver_.get());
  SetUpInProcessBrowserTestFixture();

  base::Closure* ui_task =
      new base::Closure(
          base::Bind(&BrowserTestBase::ProxyRunTestOnMainThreadLoop, this));

#if defined(OS_ANDROID)
  MainFunctionParams params(*command_line);
  params.ui_task = ui_task;
  // TODO(phajdan.jr): Check return code, http://crbug.com/374738 .
  BrowserMain(params);
#else
  GetContentMainParams()->ui_task = ui_task;
  EXPECT_EQ(expected_exit_code_, ContentMain(*GetContentMainParams()));
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

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableTracing)) {
    base::debug::CategoryFilter category_filter(
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableTracing));
    TracingController::GetInstance()->EnableRecording(
        category_filter,
        base::debug::TraceOptions(base::debug::RECORD_CONTINUOUSLY),
        TracingController::EnableRecordingDoneCallback());
  }

  RunTestOnMainThreadLoop();

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableTracing)) {
    base::FilePath trace_file =
        CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kEnableTracingOutput);
    // If there was no file specified, put a hardcoded one in the current
    // working directory.
    if (trace_file.empty())
      trace_file = base::FilePath().AppendASCII("trace.json");

    // Wait for tracing to collect results from the renderers.
    base::RunLoop run_loop;
    TracingController::GetInstance()->DisableRecording(
        TracingControllerImpl::CreateFileSink(
            trace_file,
            base::Bind(&TraceDisableRecordingComplete,
                       run_loop.QuitClosure(),
                       trace_file)));
    run_loop.Run();
  }
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

void BrowserTestBase::EnablePixelOutput() { enable_pixel_output_ = true; }

void BrowserTestBase::UseSoftwareCompositing() {
  use_software_compositing_ = true;
}

bool BrowserTestBase::UsingOSMesa() const {
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  return cmd->GetSwitchValueASCII(switches::kUseGL) ==
         gfx::kGLImplementationOSMesaName;
}

}  // namespace content
