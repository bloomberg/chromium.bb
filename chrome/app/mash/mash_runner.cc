// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/mash/mash_runner.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/common/content_switches.h"
#include "mash/package/mash_packaged_service.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/background/background_shell.h"
#include "services/shell/native_runner_delegate.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/public/interfaces/service_factory.mojom.h"
#include "services/shell/runner/common/switches.h"
#include "services/shell/runner/host/child_process_base.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

using shell::mojom::ServiceFactory;

namespace {

// kProcessType used to identify child processes.
const char* kMashChild = "mash-child";

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

// Convert the command line program from chrome_mash to chrome. This is
// necessary as the shell will attempt to start chrome_mash. We want chrome.
void ChangeChromeMashToChrome(base::CommandLine* command_line) {
  base::FilePath exe_path(command_line->GetProgram());
#if defined(OS_WIN)
  exe_path = exe_path.DirName().Append(FILE_PATH_LITERAL("chrome.exe"));
#else
  exe_path = exe_path.DirName().Append(FILE_PATH_LITERAL("chrome"));
#endif
  command_line->SetProgram(exe_path);
}

class NativeRunnerDelegateImpl : public shell::NativeRunnerDelegate {
 public:
  NativeRunnerDelegateImpl() {}
  ~NativeRunnerDelegateImpl() override {}

 private:
  // shell::NativeRunnerDelegate:
  void AdjustCommandLineArgumentsForTarget(
      const shell::Identity& target,
      base::CommandLine* command_line) override {
    if (target.name() != "exe:chrome") {
      if (target.name() == "exe:chrome_mash")
        ChangeChromeMashToChrome(command_line);
      command_line->AppendSwitchASCII(switches::kProcessType, kMashChild);
#if defined(OS_WIN)
      command_line->AppendArg(switches::kPrefetchArgumentOther);
#endif
      return;
    }

    base::CommandLine::StringVector argv(command_line->argv());
    auto iter =
        std::find(argv.begin(), argv.end(), FILE_PATH_LITERAL("--mash"));
    if (iter != argv.end())
      argv.erase(iter);
    *command_line = base::CommandLine(argv);
  }

  DISALLOW_COPY_AND_ASSIGN(NativeRunnerDelegateImpl);
};

}  // namespace

MashRunner::MashRunner() {}

MashRunner::~MashRunner() {}

void MashRunner::Run() {
  if (IsChild())
    RunChild();
  else
    RunMain();
}

void MashRunner::RunMain() {
  // TODO(sky): refactor backgroundshell so can supply own context, we
  // shouldn't we using context as it has a lot of stuff we don't really want
  // in chrome.
  NativeRunnerDelegateImpl native_runner_delegate;
  shell::BackgroundShell background_shell;
  std::unique_ptr<shell::BackgroundShell::InitParams> init_params(
      new shell::BackgroundShell::InitParams);
  init_params->native_runner_delegate = &native_runner_delegate;
  background_shell.Init(std::move(init_params));
  service_.reset(new mash::MashPackagedService);
  service_->set_context(base::MakeUnique<shell::ServiceContext>(
      service_.get(),
      background_shell.CreateServiceRequest("exe:chrome_mash")));
  service_->connector()->Connect("service:mash_session");
  base::RunLoop().Run();
}

void MashRunner::RunChild() {
  base::i18n::InitializeICU();
  InitializeResources();
  shell::ChildProcessMainWithCallback(
      base::Bind(&MashRunner::StartChildApp, base::Unretained(this)));
}

void MashRunner::StartChildApp(
    shell::mojom::ServiceRequest service_request) {
  // TODO(sad): Normally, this would be a TYPE_DEFAULT message loop. However,
  // TYPE_UI is needed for mojo:ui. But it is not known whether the child app is
  // going to be mojo:ui at this point. So always create a TYPE_UI message loop
  // for now.
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  service_.reset(new mash::MashPackagedService);
  service_->set_context(base::MakeUnique<shell::ServiceContext>(
      service_.get(), std::move(service_request)));
  base::RunLoop().Run();
}

int MashMain() {
#if defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif
  // TODO(sky): wire this up correctly.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
  // To view log output with IDs and timestamps use "adb logcat -v threadtime".
  logging::SetLogItems(true,   // Process ID
                       true,   // Thread ID
                       true,   // Timestamp
                       true);  // Tick count

  std::unique_ptr<base::MessageLoop> message_loop;
#if defined(OS_LINUX)
  base::AtExitManager exit_manager;
#endif
  if (!IsChild())
    message_loop.reset(new base::MessageLoop(base::MessageLoop::TYPE_UI));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTraceToConsole)) {
    base::trace_event::TraceConfig trace_config =
        tracing::GetConfigForTraceToConsole();
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config,
        base::trace_event::TraceLog::RECORDING_MODE);
  }

  MashRunner mash_runner;
  mash_runner.Run();
  return 0;
}
