// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/mash/mash_runner.h"

#include <string>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/sys_info.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "chrome/app/mash/chrome_mash_catalog.h"
#include "chrome/common/chrome_switches.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "mash/common/config.h"
#include "mash/package/mash_packaged_service.h"
#include "mash/quick_launch/public/interfaces/constants.mojom.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/catalog/public/interfaces/constants.mojom.h"
#include "services/service_manager/background/background_service_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/standalone_service/standalone_service.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/runner/init.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_POSIX)
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/app/shutdown_signal_handlers_posix.h"
#endif

using service_manager::mojom::ServiceFactory;

namespace {

// kProcessType used to identify child processes.
const char* kMashChild = "mash-child";

const char kChromeMashServiceName[] = "chrome_mash";

bool IsChild() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kProcessType) &&
         base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kProcessType) == kMashChild;
}

void InitializeResources() {
  ui::RegisterPathProvider();
  const std::string locale =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kLang);
  // This loads the Chrome's resources (chrome_100_percent.pak etc.)
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      locale, nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
}

class ServiceProcessLauncherDelegateImpl
    : public service_manager::ServiceProcessLauncher::Delegate {
 public:
  ServiceProcessLauncherDelegateImpl() {}
  ~ServiceProcessLauncherDelegateImpl() override {}

 private:
  // service_manager::ServiceProcessLauncher::Delegate:
  void AdjustCommandLineArgumentsForTarget(
        const service_manager::Identity& target,
        base::CommandLine* command_line) override {
    if (target.name() == kChromeMashServiceName ||
        target.name() == content::mojom::kPackagedServicesServiceName) {
      base::FilePath exe_path;
      base::PathService::Get(base::FILE_EXE, &exe_path);
      command_line->SetProgram(exe_path);
    }
    if (target.name() != content::mojom::kPackagedServicesServiceName) {
      // If running anything other than the browser process, launch a mash
      // child process. The new process will execute MashRunner::RunChild().
      command_line->AppendSwitchASCII(switches::kProcessType, kMashChild);
#if defined(OS_WIN)
      command_line->AppendArg(switches::kPrefetchArgumentOther);
#endif
      return;
    }

    // When launching the browser process, ensure that we don't inherit the
    // --mash flag so it proceeds with the normal content/browser startup path.
    base::CommandLine::SwitchMap new_switches = command_line->GetSwitches();
    new_switches.erase(switches::kMash);
    *command_line = base::CommandLine(command_line->GetProgram());
    for (const std::pair<std::string, base::CommandLine::StringType>& sw :
         new_switches) {
      command_line->AppendSwitchNative(sw.first, sw.second);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessLauncherDelegateImpl);
};

// Quits |run_loop| if the |identity| of the quitting service is critical to the
// system (e.g. the window manager). Used in the main process.
void OnInstanceQuitInMain(base::RunLoop* run_loop,
                          int* exit_value,
                          const service_manager::Identity& identity) {
  DCHECK(exit_value);
  DCHECK(run_loop);

  if (identity.name() != mash::common::GetWindowManagerServiceName() &&
      identity.name() != ui::mojom::kServiceName &&
      identity.name() != content::mojom::kPackagedServicesServiceName) {
    return;
  }

  LOG(ERROR) << "Main process exiting because service " << identity.name()
             << " quit unexpectedly.";
  *exit_value = 1;
  run_loop->Quit();
}

}  // namespace

MashRunner::MashRunner() {}

MashRunner::~MashRunner() {}

int MashRunner::Run() {
  base::TaskScheduler::CreateAndSetSimpleTaskScheduler(
      base::SysInfo::NumberOfProcessors());

  if (IsChild())
    return RunChild();

  return RunMain();
}

int MashRunner::RunMain() {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);

  base::SequencedWorkerPool::EnableWithRedirectionToTaskSchedulerForProcess();

  mojo::edk::Init();

  base::Thread ipc_thread("IPC thread");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  mojo::edk::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::edk::ScopedIPCSupport::ShutdownPolicy::FAST);

  int exit_value = RunServiceManagerInMain();

  ipc_thread.Stop();
  base::TaskScheduler::GetInstance()->Shutdown();
  return exit_value;
}

int MashRunner::RunServiceManagerInMain() {
  // TODO(sky): refactor BackgroundServiceManager so can supply own context, we
  // shouldn't we using context as it has a lot of stuff we don't really want
  // in chrome.
  ServiceProcessLauncherDelegateImpl service_process_launcher_delegate;
  service_manager::BackgroundServiceManager background_service_manager(
      &service_process_launcher_delegate, CreateChromeMashCatalog());
  service_manager::mojom::ServicePtr service;
  service_manager::ServiceContext context(
      base::MakeUnique<mash::MashPackagedService>(),
      service_manager::mojom::ServiceRequest(&service));
  background_service_manager.RegisterService(
      service_manager::Identity(
          kChromeMashServiceName, service_manager::mojom::kRootUserID),
      std::move(service), nullptr);

  // Quit the main process if an important child (e.g. window manager) dies.
  // On Chrome OS the OS-level session_manager will restart the main process.
  base::RunLoop run_loop;
  int exit_value = 0;
  background_service_manager.SetInstanceQuitCallback(
      base::Bind(&OnInstanceQuitInMain, &run_loop, &exit_value));

#if defined(OS_POSIX)
  // Quit the main process in response to shutdown signals (like SIGTERM).
  // These signals are used by Linux distributions to request clean shutdown.
  // On Chrome OS the SIGTERM signal is sent by session_manager.
  InstallShutdownSignalHandlers(run_loop.QuitClosure(),
                                base::ThreadTaskRunnerHandle::Get());
#endif

  // Ping services that we know we want to launch on startup (UI service,
  // window manager, quick launch app).
  context.connector()->Connect(ui::mojom::kServiceName);
  context.connector()->Connect(mash::common::GetWindowManagerServiceName());
  context.connector()->Connect(mash::quick_launch::mojom::kServiceName);
  context.connector()->Connect(content::mojom::kPackagedServicesServiceName);

  run_loop.Run();

  // |context| must be destroyed before the message loop.
  return exit_value;
}

int MashRunner::RunChild() {
  service_manager::WaitForDebuggerIfNecessary();

  base::i18n::InitializeICU();
  InitializeResources();
  service_manager::RunStandaloneService(
      base::Bind(&MashRunner::StartChildApp, base::Unretained(this)));
  return 0;
}

void MashRunner::StartChildApp(
    service_manager::mojom::ServiceRequest service_request) {
  // TODO(sad): Normally, this would be a TYPE_DEFAULT message loop. However,
  // TYPE_UI is needed for mojo:ui. But it is not known whether the child app is
  // going to be mojo:ui at this point. So always create a TYPE_UI message loop
  // for now.
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  base::RunLoop run_loop;
  service_manager::ServiceContext context(
      base::MakeUnique<mash::MashPackagedService>(),
      std::move(service_request));
  // Quit the child process when the service quits.
  context.SetQuitClosure(run_loop.QuitClosure());
  run_loop.Run();
  // |context| must be destroyed before |message_loop|.
}

int MashMain() {
#if !defined(OFFICIAL_BUILD) && defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif
  // TODO(sky): wire this up correctly.
  service_manager::InitializeLogging();

#if defined(OS_LINUX)
  base::AtExitManager exit_manager;
#endif

#if !defined(OFFICIAL_BUILD)
  // Initialize stack dumping before initializing sandbox to make sure symbol
  // names in all loaded libraries will be cached.
  // NOTE: On Chrome OS, crash reporting for the root process and non-browser
  // service processes is handled by the OS-level crash_reporter.
  base::debug::EnableInProcessStackDumping();
#endif

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTraceToConsole)) {
    base::trace_event::TraceConfig trace_config =
        tracing::GetConfigForTraceToConsole();
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config,
        base::trace_event::TraceLog::RECORDING_MODE);
  }

  MashRunner mash_runner;
  return mash_runner.Run();
}

void WaitForMashDebuggerIfNecessary() {
  if (!service_manager::ServiceManagerIsRemote())
    return;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const std::string service_name =
      command_line->GetSwitchValueASCII(switches::kProcessServiceName);
  if (service_name !=
      command_line->GetSwitchValueASCII(switches::kWaitForDebugger)) {
    return;
  }

  // Include the pid as logging may not have been initialized yet (the pid
  // printed out by logging is wrong).
  LOG(WARNING) << "waiting for debugger to attach for service " << service_name
               << " pid=" << base::Process::Current().Pid();
  base::debug::WaitForDebugger(120, true);
}
