// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/mash/mash_runner.h"

#include "ash/mus/sysui_application.h"
#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "components/mus/mus_app.h"
#include "components/resource_provider/resource_provider_app.h"
#include "content/public/common/content_switches.h"
#include "mash/browser_driver/browser_driver_application_delegate.h"
#include "mash/quick_launch/quick_launch_application.h"
#include "mash/session/session.h"
#include "mash/task_viewer/task_viewer.h"
#include "mash/wm/window_manager_application.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/background/background_shell.h"
#include "services/shell/native_runner_delegate.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "services/shell/public/interfaces/shell_client_factory.mojom.h"
#include "services/shell/runner/common/switches.h"
#include "services/shell/runner/host/child_process_base.h"

#if defined(OS_LINUX)
#include "components/font_service/font_service_app.h"
#endif

using shell::mojom::ShellClientFactory;

namespace {

// kProcessType used to identify child processes.
const char* kMashChild = "mash-child";

// ShellClient responsible for starting the appropriate app.
class DefaultShellClient : public shell::ShellClient,
                           public ShellClientFactory,
                           public shell::InterfaceFactory<ShellClientFactory> {
 public:
  DefaultShellClient() {}
  ~DefaultShellClient() override {}

  // shell::ShellClient:
  bool AcceptConnection(shell::Connection* connection) override {
    connection->AddInterface<ShellClientFactory>(this);
    return true;
  }

  // shell::InterfaceFactory<ShellClientFactory>
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<ShellClientFactory> request) override {
    shell_client_factory_bindings_.AddBinding(this, std::move(request));
  }

  // ShellClientFactory:
  void CreateShellClient(shell::mojom::ShellClientRequest request,
                         const mojo::String& mojo_name) override {
    if (shell_client_) {
      LOG(ERROR) << "request to create additional app " << mojo_name;
      return;
    }
    shell_client_ = CreateShellClient(mojo_name);
    if (shell_client_) {
      shell_connection_.reset(
          new shell::ShellConnection(shell_client_.get(), std::move(request)));
      return;
    }
    LOG(ERROR) << "unknown name " << mojo_name;
    NOTREACHED();
  }

 private:
  // TODO(sky): move this into mash.
  std::unique_ptr<shell::ShellClient> CreateShellClient(
      const std::string& name) {
    if (name == "mojo:ash_sysui")
      return base::WrapUnique(new ash::sysui::SysUIApplication);
    if (name == "mojo:desktop_wm")
      return base::WrapUnique(new mash::wm::WindowManagerApplication);
    if (name == "mojo:mash_session")
      return base::WrapUnique(new mash::session::Session);
    if (name == "mojo:mus")
      return base::WrapUnique(new mus::MandolineUIServicesApp);
    if (name == "mojo:quick_launch")
      return base::WrapUnique(new mash::quick_launch::QuickLaunchApplication);
    if (name == "mojo:resource_provider") {
      return base::WrapUnique(
          new resource_provider::ResourceProviderApp("mojo:resource_provider"));
    }
    if (name == "mojo:task_viewer")
      return base::WrapUnique(new mash::task_viewer::TaskViewer);
#if defined(OS_LINUX)
    if (name == "mojo:font_service")
      return base::WrapUnique(new font_service::FontServiceApp);
#endif
    if (name == "mojo:browser_driver") {
      return base::WrapUnique(
          new mash::browser_driver::BrowserDriverApplicationDelegate());
    }
    return nullptr;
  }

  mojo::BindingSet<ShellClientFactory> shell_client_factory_bindings_;
  std::unique_ptr<shell::ShellClient> shell_client_;
  std::unique_ptr<shell::ShellConnection> shell_connection_;

  DISALLOW_COPY_AND_ASSIGN(DefaultShellClient);
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
  shell_client_.reset(new DefaultShellClient);
  shell_connection_.reset(new shell::ShellConnection(
      shell_client_.get(),
      background_shell.CreateShellClientRequest("exe:chrome_mash")));
  shell_connection_->connector()->Connect("mojo:mash_session");
  base::MessageLoop::current()->Run();
}

void MashRunner::RunChild() {
  base::i18n::InitializeICU();
  shell::ChildProcessMain(
      base::Bind(&MashRunner::StartChildApp, base::Unretained(this)));
}

void MashRunner::StartChildApp(
    shell::mojom::ShellClientRequest client_request) {
  // TODO(sky): use MessagePumpMojo.
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  shell_client_.reset(new DefaultShellClient);
  shell_connection_.reset(new shell::ShellConnection(
      shell_client_.get(), std::move(client_request)));
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
