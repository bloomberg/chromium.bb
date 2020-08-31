// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/ui/policy/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
#include <lib/vfs/cpp/pseudo_file.h>
#include <iostream>

#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/values.h"
#include "fuchsia/base/init_logging.h"
#include "fuchsia/base/release_channel.h"
#include "url/gurl.h"

fuchsia::sys::ComponentControllerPtr component_controller_;

namespace {

constexpr char kRemoteDebuggingPortSwitch[] = "remote-debugging-port";
constexpr char kHeadlessSwitch[] = "headless";

void PrintUsage() {
  std::cerr << "Usage: "
            << base::CommandLine::ForCurrentProcess()->GetProgram().BaseName()
            << " [--" << kRemoteDebuggingPortSwitch << "] [--"
            << kHeadlessSwitch << "] URL. [--] [--{extra_flag1}] "
            << "[--{extra_flag2}]" << std::endl
            << "Setting " << kRemoteDebuggingPortSwitch << " to 0 will "
            << "automatically choose an available port." << std::endl
            << "Setting " << kHeadlessSwitch << " will prevent creation of "
            << "a view." << std::endl
            << "Extra flags will be passed to "
            << "WebEngine to be processed." << std::endl;
}

base::Optional<uint16_t> ParseRemoteDebuggingPort(
    const base::CommandLine& command_line) {
  std::string port_str =
      command_line.GetSwitchValueNative(kRemoteDebuggingPortSwitch);
  int port_parsed;
  if (!base::StringToInt(port_str, &port_parsed) || port_parsed < 0 ||
      port_parsed > 65535) {
    LOG(ERROR) << "Invalid value for --remote-debugging-port (must be in the "
                  "range 0-65535).";
    return base::nullopt;
  }
  return (uint16_t)port_parsed;
}

GURL GetUrlFromArgs(const base::CommandLine::StringVector& args) {
  if (args.empty()) {
    LOG(ERROR) << "No URL provided.";
    return GURL();
  }
  GURL url = GURL(args.front());
  if (!url.is_valid()) {
    LOG(ERROR) << "URL is not valid: " << url.spec();
    return GURL();
  }
  return url;
}

fuchsia::web::ContextProviderPtr ConnectToContextProvider(
    const base::CommandLine::StringVector& extra_command_line_arguments) {
  sys::ComponentContext* const component_context =
      base::fuchsia::ComponentContextForCurrentProcess();

  // If there are no additional command-line arguments then use the
  // system instance of the ContextProvider.
  if (extra_command_line_arguments.empty()) {
    return component_context->svc()->Connect<fuchsia::web::ContextProvider>();
  }

  // Launch a private ContextProvider instance, with the desired command-line
  // arguments.
  fuchsia::sys::LauncherPtr launcher;
  component_context->svc()->Connect(launcher.NewRequest());

  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url = base::StrCat({"fuchsia-pkg://fuchsia.com/web_engine",
                                  BUILDFLAG(FUCHSIA_RELEASE_CHANNEL_SUFFIX),
                                  "#meta/context_provider.cmx"});

  launch_info.arguments = extra_command_line_arguments;
  fidl::InterfaceHandle<fuchsia::io::Directory> service_directory;
  launch_info.directory_request = service_directory.NewRequest().TakeChannel();

  launcher->CreateComponent(std::move(launch_info),
                            component_controller_.NewRequest());

  sys::ServiceDirectory web_engine_service_dir(std::move(service_directory));
  return web_engine_service_dir.Connect<fuchsia::web::ContextProvider>();
}

}  // namespace

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);

  // Parse the command line arguments and set up logging.
  CHECK(base::CommandLine::Init(argc, argv));
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Set logging to stderr if not specified.
  if (!command_line->HasSwitch(cr_fuchsia::kEnableLogging)) {
    command_line->AppendSwitchNative(cr_fuchsia::kEnableLogging, "stderr");
  }
  CHECK(cr_fuchsia::InitLoggingFromCommandLine(*command_line));

  base::Optional<uint16_t> remote_debugging_port;
  if (command_line->HasSwitch(kRemoteDebuggingPortSwitch)) {
    remote_debugging_port = ParseRemoteDebuggingPort(*command_line);
    if (!remote_debugging_port) {
      PrintUsage();
      return 1;
    }
  }
  bool is_headless = command_line->HasSwitch(kHeadlessSwitch);

  base::CommandLine::StringVector additional_args = command_line->GetArgs();
  GURL url(GetUrlFromArgs(additional_args));
  if (!url.is_valid()) {
    PrintUsage();
    return 1;
  }

  // Remove the url since we don't pass it into WebEngine
  additional_args.erase(additional_args.begin());

  fuchsia::web::ContextProviderPtr web_context_provider =
      ConnectToContextProvider(additional_args);

  // Set up the content directory fuchsia-pkg://shell-data/, which will host
  // the files stored under //fuchsia/engine/test/shell_data.
  fuchsia::web::CreateContextParams create_context_params;
  fuchsia::web::ContentDirectoryProvider content_directory;
  base::FilePath pkg_path;
  base::PathService::Get(base::DIR_ASSETS, &pkg_path);
  content_directory.set_directory(base::fuchsia::OpenDirectory(
      pkg_path.AppendASCII("fuchsia/engine/test/shell_data")));
  content_directory.set_name("shell-data");
  std::vector<fuchsia::web::ContentDirectoryProvider> content_directories;
  content_directories.emplace_back(std::move(content_directory));
  create_context_params.set_content_directories(
      {std::move(content_directories)});

  // WebEngine Contexts can only make use of the services provided by the
  // embedder application. By passing a handle to this process' service
  // directory to the ContextProvider, we are allowing the Context access to the
  // same set of services available to this application.
  create_context_params.set_service_directory(base::fuchsia::OpenDirectory(
      base::FilePath(base::fuchsia::kServiceDirectoryPath)));

  // Enable other WebEngine features.
  fuchsia::web::ContextFeatureFlags features =
      fuchsia::web::ContextFeatureFlags::AUDIO |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;
  if (is_headless)
    features |= fuchsia::web::ContextFeatureFlags::HEADLESS;
  else
    features |= fuchsia::web::ContextFeatureFlags::VULKAN;

  create_context_params.set_features(features);
  if (remote_debugging_port)
    create_context_params.set_remote_debugging_port(*remote_debugging_port);

  base::RunLoop run_loop;

  // Create the browser |context|.
  fuchsia::web::ContextPtr context;
  web_context_provider->Create(std::move(create_context_params),
                               context.NewRequest());
  context.set_error_handler(
      [quit_run_loop = run_loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "Context connection lost:";
        quit_run_loop.Run();
      });

  // Create the browser |frame| which will contain the webpage.
  fuchsia::web::CreateFrameParams frame_params;
  if (remote_debugging_port)
    frame_params.set_enable_remote_debugging(true);

  fuchsia::web::FramePtr frame;
  context->CreateFrameWithParams(std::move(frame_params), frame.NewRequest());
  frame.set_error_handler(
      [quit_run_loop = run_loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "Frame connection lost:";
        quit_run_loop.Run();
      });

  // Log the debugging port, if debugging is requested.
  if (remote_debugging_port) {
    context->GetRemoteDebuggingPort(
        [](fuchsia::web::Context_GetRemoteDebuggingPort_Result result) {
          if (result.is_err()) {
            LOG(ERROR) << "Remote debugging service was not opened.";
            return;
          }
          // Telemetry expects this exact format of log line output to retrieve
          // the remote debugging port.
          LOG(INFO) << "Remote debugging port: " << result.response().port;
        });
  }

  // Navigate |frame| to |url|.
  fuchsia::web::LoadUrlParams load_params;
  load_params.set_type(fuchsia::web::LoadUrlReason::TYPED);
  load_params.set_was_user_activated(true);
  fuchsia::web::NavigationControllerPtr nav_controller;
  frame->GetNavigationController(nav_controller.NewRequest());
  nav_controller->LoadUrl(
      url.spec(), std::move(load_params),
      [quit_run_loop = run_loop.QuitClosure()](
          fuchsia::web::NavigationController_LoadUrl_Result result) {
        if (result.is_err()) {
          LOG(ERROR) << "LoadUrl failed.";
          quit_run_loop.Run();
        }
      });

  if (is_headless)
    frame->EnableHeadlessRendering();
  else {
    // Present a fullscreen view of |frame|.
    fuchsia::ui::views::ViewToken view_token;
    fuchsia::ui::views::ViewHolderToken view_holder_token;
    std::tie(view_token, view_holder_token) = scenic::NewViewTokenPair();
    frame->CreateView(std::move(view_token));
    auto presenter = base::fuchsia::ComponentContextForCurrentProcess()
                         ->svc()
                         ->Connect<::fuchsia::ui::policy::Presenter>();
    presenter->PresentView(std::move(view_holder_token), nullptr);
  }

  LOG(INFO) << "Launched browser at URL " << url.spec();

  // Run until the process is killed with CTRL-C or the connections to Web
  // Engine interfaces are dropped.
  run_loop.Run();

  return 0;
}
