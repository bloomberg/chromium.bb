// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_base.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/i18n/icu_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/network_service_test.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "content/test/content_browser_sanity_checker.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/platform_window_defaults.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_POSIX)
#include "base/process/process_handle.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif

#if defined(USE_AURA)
#include "content/browser/compositor/image_transport_factory.h"
#include "ui/aura/test/event_generator_delegate_aura.h"  // nogncheck
#endif

namespace content {
namespace {

#if defined(OS_POSIX) && !defined(OS_FUCHSIA)
// On SIGSEGV or SIGTERM (sent by the runner on timeouts), dump a stack trace
// (to make debugging easier) and also exit with a known error code (so that
// the test framework considers this a failure -- http://crbug.com/57578).
// Note: We only want to do this in the browser process, and not forked
// processes. That might lead to hangs because of locks inside tcmalloc or the
// OS. See http://crbug.com/141302.
int g_browser_process_pid;

void DumpStackTraceSignalHandler(int signal) {
  if (g_browser_process_pid == base::GetCurrentProcId()) {
    std::string message("BrowserTestBase received signal: ");
    message += strsignal(signal);
    message += ". Backtrace:\n";
    logging::RawLog(logging::LOG_ERROR, message.c_str());
    base::debug::StackTrace().Print();
  }
  _exit(128 + signal);
}
#endif  // defined(OS_POSIX) && !defined(OS_FUCHSIA)

void RunTaskOnRendererThread(const base::Closure& task,
                             const base::Closure& quit_task) {
  task.Run();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_task);
}

void TraceStopTracingComplete(const base::Closure& quit,
                                   const base::FilePath& file_path) {
  LOG(ERROR) << "Tracing written to: " << file_path.value();
  quit.Run();
}

}  // namespace

extern int BrowserMain(const MainFunctionParams&);

BrowserTestBase::BrowserTestBase()
    : expected_exit_code_(0),
      enable_pixel_output_(false),
      use_software_compositing_(false),
      set_up_called_(false),
      disable_io_checks_(false) {
#if defined(OS_MACOSX)
  base::mac::SetOverrideAmIBundled(true);
#endif

  ui::test::EnableTestConfigForPlatformWindows();

#if defined(OS_POSIX)
  handle_sigterm_ = true;
#endif

  // This is called through base::TestSuite initially. It'll also be called
  // inside BrowserMain, so tell the code to ignore the check that it's being
  // called more than once
  base::i18n::AllowMultipleInitializeCallsForTesting();

  embedded_test_server_ = base::MakeUnique<net::EmbeddedTestServer>();

  // SequencedWorkerPool is enabled by default in tests (see
  // base::TestSuite::Initialize). In browser tests, disable it and expect it
  // to be re-enabled as part of BrowserMainLoop::PreCreateThreads().
  // TODO(fdoray): Remove this once the SequencedWorkerPool to TaskScheduler
  // redirection experiment concludes https://crbug.com/622400.
  base::SequencedWorkerPool::DisableForProcessForTesting();
}

BrowserTestBase::~BrowserTestBase() {
#if defined(OS_ANDROID)
  // RemoteTestServer can cause wait on the UI thread.
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  spawned_test_server_.reset();
#endif

  CHECK(set_up_called_) << "SetUp was not called. This probably means that the "
                           "developer has overridden the method and not called "
                           "the superclass version. In this case, the test "
                           "does not run and reports a false positive result.";
}

void BrowserTestBase::SetUp() {
  set_up_called_ = true;
  // ContentTestSuiteBase might have already initialized
  // MaterialDesignController in browser_tests suite.
  // Uninitialize here to let the browser process do it.
  ui::test::MaterialDesignControllerTestAPI::Uninitialize();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Override the child process connection timeout since tests can exceed that
  // when sharded.
  command_line->AppendSwitchASCII(
      switches::kIPCConnectionTimeout,
      base::Int64ToString(TestTimeouts::action_max_timeout().InSeconds()));

  // The tests assume that file:// URIs can freely access other file:// URIs.
  command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);

  command_line->AppendSwitch(switches::kDomAutomationController);

  // It is sometimes useful when looking at browser test failures to know which
  // GPU blacklisting decisions were made.
  command_line->AppendSwitch(switches::kLogGpuControlListDecisions);

  if (use_software_compositing_)
    command_line->AppendSwitch(switches::kDisableGpu);

  // The layout of windows on screen is unpredictable during tests, so disable
  // occlusion when running browser tests.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableBackgroundingOccludedWindowsForTesting);

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

  aura::test::InitializeAuraEventGeneratorDelegate();
#endif

  bool use_software_gl = true;

  // We usually use software GL as this works on all bots. The command
  // line can override this behaviour to use hardware GL.
  if (command_line->HasSwitch(switches::kUseGpuInTests))
    use_software_gl = false;

  // Some bots pass this flag when they want to use hardware GL.
  if (command_line->HasSwitch("enable-gpu"))
    use_software_gl = false;

#if defined(OS_MACOSX)
  // On Mac we always use hardware GL.
  use_software_gl = false;
#endif

#if defined(OS_ANDROID)
  // On Android we always use hardware GL.
  use_software_gl = false;
#endif

#if defined(OS_CHROMEOS)
  // If the test is running on the chromeos envrionment (such as
  // device or vm bots), we use hardware GL.
  if (base::SysInfo::IsRunningOnChromeOS())
    use_software_gl = false;
#endif

  if (use_software_gl && !use_software_compositing_)
    command_line->AppendSwitch(switches::kOverrideUseSoftwareGLForTests);

  // Use an sRGB color profile to ensure that the machine's color profile does
  // not affect the results.
  command_line->AppendSwitchASCII(switches::kForceColorProfile, "srgb");

  test_host_resolver_ = base::MakeUnique<TestHostResolver>();

  ContentBrowserSanityChecker scoped_enable_sanity_checks;

  SetUpInProcessBrowserTestFixture();

  // At this point, copy features to the command line, since BrowserMain will
  // wipe out the current feature list.
  std::string enabled_features;
  std::string disabled_features;
  if (base::FeatureList::GetInstance()) {
    base::FeatureList::GetInstance()->GetFeatureOverrides(&enabled_features,
                                                          &disabled_features);
  }

  if (!enabled_features.empty()) {
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    enabled_features);
  }
  if (!disabled_features.empty()) {
    command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                    disabled_features);
  }

  // Need to wipe feature list clean, since BrowserMain calls
  // FeatureList::SetInstance, which expects no instance to exist.
  base::FeatureList::ClearInstanceForTesting();

  auto ui_task = base::MakeUnique<base::Closure>(base::Bind(
      &BrowserTestBase::ProxyRunTestOnMainThreadLoop, base::Unretained(this)));

#if defined(OS_ANDROID)
  MainFunctionParams params(*command_line);
  params.ui_task = ui_task.release();
  // TODO(phajdan.jr): Check return code, http://crbug.com/374738 .
  BrowserMain(params);
#else
  GetContentMainParams()->ui_task = ui_task.release();
  EXPECT_EQ(expected_exit_code_, ContentMain(*GetContentMainParams()));
#endif
  TearDownInProcessBrowserTestFixture();
}

void BrowserTestBase::TearDown() {
}

void BrowserTestBase::ProxyRunTestOnMainThreadLoop() {
#if defined(OS_POSIX) && !defined(OS_FUCHSIA)
  g_browser_process_pid = base::GetCurrentProcId();
  signal(SIGSEGV, DumpStackTraceSignalHandler);

  if (handle_sigterm_)
    signal(SIGTERM, DumpStackTraceSignalHandler);
#endif  // defined(OS_POSIX) && !defined(OS_FUCHSIA)

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableTracing)) {
    base::trace_event::TraceConfig trace_config(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableTracing),
        base::trace_event::RECORD_CONTINUOUSLY);
    TracingController::GetInstance()->StartTracing(
        trace_config,
        TracingController::StartTracingDoneCallback());
  }

  {
    // This can be called from a posted task. Allow nested tasks here, because
    // otherwise the test body will have to do it in order to use RunLoop for
    // waiting.
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    PreRunTestOnMainThread();
    SetUpOnMainThread();

    // Tests would have added their host_resolver() rules by now, so copy them
    // to the network process if it's in use.
    InitializeNetworkProcess();

    bool old_io_allowed_value = false;
    if (!disable_io_checks_)
      old_io_allowed_value = base::ThreadRestrictions::SetIOAllowed(false);
    RunTestOnMainThread();
    if (!disable_io_checks_)
      base::ThreadRestrictions::SetIOAllowed(old_io_allowed_value);
    TearDownOnMainThread();
    PostRunTestOnMainThread();
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableTracing)) {
    base::FilePath trace_file =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kEnableTracingOutput);
    // If there was no file specified, put a hardcoded one in the current
    // working directory.
    if (trace_file.empty())
      trace_file = base::FilePath().AppendASCII("trace.json");

    // Wait for tracing to collect results from the renderers.
    base::RunLoop run_loop;
    TracingController::GetInstance()->StopTracing(
        TracingControllerImpl::CreateFileEndpoint(
            trace_file, base::Bind(&TraceStopTracingComplete,
                                   run_loop.QuitClosure(), trace_file)));
    run_loop.Run();
  }
}

void BrowserTestBase::CreateTestServer(const base::FilePath& test_server_base) {
  CHECK(!spawned_test_server_.get());
  spawned_test_server_ = base::MakeUnique<net::SpawnedTestServer>(
      net::SpawnedTestServer::TYPE_HTTP, test_server_base);
  embedded_test_server()->AddDefaultHandlers(test_server_base);
}

void BrowserTestBase::PostTaskToInProcessRendererAndWait(
    const base::Closure& task) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess));

  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;

  base::MessageLoop* renderer_loop =
      RenderProcessHostImpl::GetInProcessRendererThreadForTesting();
  CHECK(renderer_loop);

  renderer_loop->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunTaskOnRendererThread, task, runner->QuitClosure()));
  runner->Run();
}

void BrowserTestBase::EnablePixelOutput() { enable_pixel_output_ = true; }

void BrowserTestBase::UseSoftwareCompositing() {
  use_software_compositing_ = true;
}

bool BrowserTestBase::UsingSoftwareGL() const {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  return cmd->GetSwitchValueASCII(switches::kUseGL) ==
         gl::GetGLImplementationName(gl::GetSoftwareGLImplementation());
}

void BrowserTestBase::InitializeNetworkProcess() {
  const testing::TestInfo* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  bool network_service =
      base::FeatureList::IsEnabled(features::kNetworkService);
  // ProcessTransferAfterError is the only browser test which needs to modify
  // the host rules (when not using the network service).
  if (network_service ||
      std::string(test_info->name()) != "ProcessTransferAfterError") {
    host_resolver()->DisableModifications();
  }

  if (!network_service)
    return;

  net::RuleBasedHostResolverProc::RuleList rules = host_resolver()->GetRules();
  std::vector<mojom::RulePtr> mojo_rules;
  for (const auto& rule : rules) {
    // For now, this covers all the rules used in content's tests.
    // TODO(jam: expand this when we try to make browser_tests and
    // components_browsertests work.
    if ((rule.resolver_type !=
             net::RuleBasedHostResolverProc::Rule::kResolverTypeSystem &&
         rule.resolver_type !=
             net::RuleBasedHostResolverProc::Rule::kResolverTypeIPLiteral) ||
        rule.address_family != net::AddressFamily::ADDRESS_FAMILY_UNSPECIFIED ||
        !!rule.latency_ms || rule.replacement.empty())
      continue;
    mojom::RulePtr mojo_rule = mojom::Rule::New();
    mojo_rule->host_pattern = rule.host_pattern;
    mojo_rule->replacement = rule.replacement;
    mojo_rules.push_back(std::move(mojo_rule));
  }

  if (mojo_rules.empty())
    return;

  mojom::NetworkServiceTestPtr network_service_test;
  ServiceManagerConnection::GetForProcess()->GetConnector()->BindInterface(
      mojom::kNetworkServiceName, &network_service_test);
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  network_service_test->AddRules(std::move(mojo_rules));
}

}  // namespace content
