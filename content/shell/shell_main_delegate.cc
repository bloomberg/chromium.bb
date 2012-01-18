// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_main_delegate.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "content/public/common/content_switches.h"
#include "content/shell/shell_content_browser_client.h"
#include "content/shell/shell_content_plugin_client.h"
#include "content/shell/shell_content_renderer_client.h"
#include "content/shell/shell_content_utility_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

ShellMainDelegate::ShellMainDelegate() {
}

ShellMainDelegate::~ShellMainDelegate() {
}

bool ShellMainDelegate::BasicStartupComplete(int* exit_code) {
  return false;
}

void ShellMainDelegate::PreSandboxStartup() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  content::SetContentClient(&content_client_);
  InitializeShellContentClient(process_type);

  InitializeResourceBundle();
}

void ShellMainDelegate::SandboxInitialized(const std::string& process_type) {
}

int ShellMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  NOTREACHED();
  return -1;
}

void ShellMainDelegate::ProcessExiting(const std::string& process_type) {
}

#if defined(OS_MACOSX)
bool ShellMainDelegate::ProcessRegistersWithSystemProcess(
    const std::string& process_type) {
  return false;
}

bool ShellMainDelegate::ShouldSendMachPort(const std::string& process_type) {
  return false;
}

bool ShellMainDelegate::DelaySandboxInitialization(
    const std::string& process_type) {
  return false;
}

#elif defined(OS_POSIX)
content::ZygoteForkDelegate* ShellMainDelegate::ZygoteStarting() {
  return NULL;
}

void ShellMainDelegate::ZygoteForked() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  InitializeShellContentClient(process_type);
}
#endif  // OS_MACOSX

void ShellMainDelegate::InitializeShellContentClient(
    const std::string& process_type) {
  if (process_type.empty()) {
    browser_client_.reset(new content::ShellContentBrowserClient);
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
  FilePath pak_dir;
  PathService::Get(base::DIR_MODULE, &pak_dir);
  PathService::Override(ui::FILE_RESOURCES_PAK,
                        pak_dir.Append(
                            FILE_PATH_LITERAL("content_shell.pak")));

  // Force the content shell to run using en-US because our ResourceBundle
  // can't run without a language and it doesn't matter.
  ui::ResourceBundle::InitSharedInstance("en-US");
}
