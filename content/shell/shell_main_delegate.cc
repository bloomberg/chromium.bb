// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_main_delegate.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/shell/shell_content_browser_client.h"
#include "content/shell/shell_content_plugin_client.h"
#include "content/shell/shell_content_renderer_client.h"
#include "content/shell/shell_content_utility_client.h"
#include "content/shell/shell_render_process_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif

#if defined(OS_MACOSX)
namespace {

void OverrideChildProcessPath() {
  if (base::mac::IsBackgroundOnlyProcess()) {
    // The background-only process is the helper; no overriding needed.
    return;
  }

  // Start out with the path to the running executable.
  FilePath helper_path;
  PathService::Get(base::FILE_EXE, &helper_path);

  // One step up to MacOS, another to Contents.
  helper_path = helper_path.DirName().DirName();
  DCHECK_EQ(helper_path.BaseName().value(), "Contents");

  // Go into the frameworks directory.
  helper_path = helper_path.Append("Frameworks");

  // And the app path.
  helper_path = helper_path.Append("Content Shell Helper.app")
                           .Append("Contents")
                           .Append("MacOS")
                           .Append("Content Shell Helper");

  PathService::Override(content::CHILD_PROCESS_EXE, helper_path);
}

}  // namespace
#endif  // OS_MACOSX

ShellMainDelegate::ShellMainDelegate() {
}

ShellMainDelegate::~ShellMainDelegate() {
}

bool ShellMainDelegate::BasicStartupComplete(int* exit_code) {
  return false;
}

void ShellMainDelegate::PreSandboxStartup() {
#if defined(OS_MACOSX)
  OverrideChildProcessPath();
#endif  // OS_MACOSX

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

  FilePath pak_file = pak_dir.Append(FILE_PATH_LITERAL("content_shell.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakFile(pak_file);
}
