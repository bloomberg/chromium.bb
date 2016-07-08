// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/mash/mash_runner.h"

#include "ash/mus/window_manager_application.h"
#include "ash/sysui/sysui_application.h"
#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "content/public/common/content_switches.h"
#include "mash/app_driver/app_driver.h"
#include "mash/quick_launch/quick_launch_application.h"
#include "mash/session/session.h"
#include "mash/task_viewer/task_viewer.h"
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
#include "services/ui/service.h"

#if defined(OS_LINUX)
#include "components/font_service/font_service_app.h"
#endif

using shell::mojom::ServiceFactory;

namespace {

// kProcessType used to identify child processes.
const char* kMashChild = "mash-child";

// Service responsible for starting the appropriate app.
class DefaultService : public shell::Service,
                       public ServiceFactory,
                       public shell::InterfaceFactory<ServiceFactory> {
 public:
  DefaultService() {}
  ~DefaultService() override {}

  // shell::Service:
  bool OnConnect(shell::Connection* connection) override {
    connection->AddInterface<ServiceFactory>(this);
    return true;
  }

  // shell::InterfaceFactory<ServiceFactory>
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<ServiceFactory> request) override {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

  // ServiceFactory:
  void CreateService(shell::mojom::ServiceRequest request,
                     const mojo::String& mojo_name) override {
    if (service_) {
      LOG(ERROR) << "request to create additional service " << mojo_name;
      return;
    }
    service_ = CreateService(mojo_name);
    if (service_) {
      shell_connection_.reset(
          new shell::ServiceContext(service_.get(), std::move(request)));
      return;
    }
    LOG(ERROR) << "unknown name " << mojo_name;
    NOTREACHED();
  }

 private:
  // TODO(sky): move this into mash.
  std::unique_ptr<shell::Service> CreateService(
      const std::string& name) {
    if (name == "mojo:ash_sysui")
      return base::WrapUnique(new ash::sysui::SysUIApplication);
    if (name == "mojo:ash")
      return base::WrapUnique(new ash::mus::WindowManagerApplication);
    if (name == "mojo:mash_session")
      return base::WrapUnique(new mash::session::Session);
    if (name == "mojo:ui")
      return base::WrapUnique(new ui::Service);
    if (name == "mojo:quick_launch")
      return base::WrapUnique(new mash::quick_launch::QuickLaunchApplication);
    if (name == "mojo:task_viewer")
      return base::WrapUnique(new mash::task_viewer::TaskViewer);
#if defined(OS_LINUX)
    if (name == "mojo:font_service")
      return base::WrapUnique(new font_service::FontServiceApp);
#endif
    if (name == "mojo:app_driver") {
      return base::WrapUnique(new mash::app_driver::AppDriver());
    }
    return nullptr;
  }

  mojo::BindingSet<ServiceFactory> service_factory_bindings_;
  std::unique_ptr<shell::Service> service_;
  std::unique_ptr<shell::ServiceContext> shell_connection_;

  DISALLOW_COPY_AND_ASSIGN(DefaultService);
};

bool IsChild() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kProcessType) &&
         base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kProcessType) == kMashChild;
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
  service_.reset(new DefaultService);
  shell_connection_.reset(new shell::ServiceContext(
      service_.get(),
      background_shell.CreateServiceRequest("exe:chrome_mash")));
  shell_connection_->connector()->Connect("mojo:mash_session");
  base::MessageLoop::current()->Run();
}

void MashRunner::RunChild() {
  base::i18n::InitializeICU();
  shell::ChildProcessMainWithCallback(
      base::Bind(&MashRunner::StartChildApp, base::Unretained(this)));
}

void MashRunner::StartChildApp(
    shell::mojom::ServiceRequest service_request) {
  // TODO(sky): use MessagePumpMojo.
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  service_.reset(new DefaultService);
  shell_connection_.reset(new shell::ServiceContext(
      service_.get(), std::move(service_request)));
  message_loop.Run();
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

  // TODO(sky): use MessagePumpMojo.
  std::unique_ptr<base::MessageLoop> message_loop;
#if defined(OS_LINUX)
  base::AtExitManager exit_manager;
#endif
  if (!IsChild())
    message_loop.reset(new base::MessageLoop(base::MessageLoop::TYPE_UI));
  MashRunner mash_runner;
  mash_runner.Run();
  return 0;
}
