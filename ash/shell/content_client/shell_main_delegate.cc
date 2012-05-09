// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content_client/shell_main_delegate.h"

#include "ash/shell/content_client/shell_content_browser_client.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/shell/shell_content_plugin_client.h"
#include "content/shell/shell_content_renderer_client.h"
#include "content/shell/shell_content_utility_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace ash {
namespace shell {
namespace {

int ShellBrowserMain(
    const content::MainFunctionParams& main_function_params) {
  scoped_ptr<content::BrowserMainRunner> main_runner(
      content::BrowserMainRunner::Create());
  int exit_code = main_runner->Initialize(main_function_params);
  if (exit_code >= 0)
    return exit_code;
  exit_code = main_runner->Run();
  main_runner->Shutdown();
  return exit_code;
}

}

ShellMainDelegate::ShellMainDelegate() {
}

ShellMainDelegate::~ShellMainDelegate() {
}

bool ShellMainDelegate::BasicStartupComplete(int* exit_code) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  content::SetContentClient(&content_client_);
  InitializeShellContentClient(process_type);

  return false;
}

void ShellMainDelegate::PreSandboxStartup() {
  InitializeResourceBundle();
}

void ShellMainDelegate::SandboxInitialized(const std::string& process_type) {
}

int ShellMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type != "")
    return -1;

  return ShellBrowserMain(main_function_params);
}

void ShellMainDelegate::ProcessExiting(const std::string& process_type) {
}

#if defined(OS_POSIX)
content::ZygoteForkDelegate* ShellMainDelegate::ZygoteStarting() {
  return NULL;
}

void ShellMainDelegate::ZygoteForked() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  InitializeShellContentClient(process_type);
}
#endif

void ShellMainDelegate::InitializeShellContentClient(
    const std::string& process_type) {
  if (process_type.empty()) {
    browser_client_.reset(new ShellContentBrowserClient);
    content::GetContentClient()->set_browser(browser_client_.get());
  } else if (process_type == switches::kRendererProcess) {
    renderer_client_.reset(new content::ShellContentRendererClient);
    content::GetContentClient()->set_renderer(renderer_client_.get());
  } else if (process_type == switches::kPluginProcess) {
    plugin_client_.reset(new content::ShellContentPluginClient);
    content::GetContentClient()->set_plugin(plugin_client_.get());
  } else if (process_type == switches::kUtilityProcess) {
    utility_client_.reset(new content::ShellContentUtilityClient);
    content::GetContentClient()->set_utility(utility_client_.get());
  }
}

void ShellMainDelegate::InitializeResourceBundle() {
  ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
}

}  // namespace shell
}  // namespace ash
