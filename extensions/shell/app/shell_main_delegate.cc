// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/app/shell_main_delegate.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/default_shell_browser_main_delegate.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/common/shell_content_client.h"
#include "extensions/shell/renderer/shell_content_renderer_client.h"
#include "extensions/shell/renderer/shell_renderer_main_delegate.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_paths.h"
#endif

namespace {

void InitLogging() {
  base::FilePath log_filename;
  PathService::Get(base::DIR_EXE, &log_filename);
  log_filename = log_filename.AppendASCII("app_shell.log");
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true, true, true, true);
}

}  // namespace

namespace extensions {

ShellMainDelegate::ShellMainDelegate() {
}

ShellMainDelegate::~ShellMainDelegate() {
}

bool ShellMainDelegate::BasicStartupComplete(int* exit_code) {
  InitLogging();
  content_client_.reset(new ShellContentClient);
  SetContentClient(content_client_.get());

#if defined(OS_CHROMEOS)
  chromeos::RegisterPathProvider();
#endif
  extensions::RegisterPathProvider();
  return false;
}

void ShellMainDelegate::PreSandboxStartup() {
  std::string process_type =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  if (ProcessNeedsResourceBundle(process_type))
    InitializeResourceBundle();
}

content::ContentBrowserClient* ShellMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(CreateShellContentBrowserClient());
  return browser_client_.get();
}

content::ContentBrowserClient*
ShellMainDelegate::CreateShellContentBrowserClient() {
  return new ShellContentBrowserClient(new DefaultShellBrowserMainDelegate());
}

content::ContentRendererClient*
ShellMainDelegate::CreateContentRendererClient() {
  renderer_client_.reset(
      new ShellContentRendererClient(CreateShellRendererMainDelegate()));
  return renderer_client_.get();
}

scoped_ptr<ShellRendererMainDelegate>
ShellMainDelegate::CreateShellRendererMainDelegate() {
  return scoped_ptr<ShellRendererMainDelegate>();
}

void ShellMainDelegate::InitializeResourceBundle() {
  base::FilePath extensions_shell_and_test_pak_path;
  PathService::Get(base::DIR_MODULE, &extensions_shell_and_test_pak_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      extensions_shell_and_test_pak_path.AppendASCII(
          "extensions_shell_and_test.pak"));
}

// static
bool ShellMainDelegate::ProcessNeedsResourceBundle(
    const std::string& process_type) {
  // The browser process has no process type flag, but needs resources.
  // On Linux the zygote process opens the resources for the renderers.
  return process_type.empty() ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kRendererProcess ||
         process_type == switches::kUtilityProcess;
}

}  // namespace extensions
