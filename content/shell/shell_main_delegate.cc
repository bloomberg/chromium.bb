// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_main_delegate.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/shell/shell_browser_main.h"
#include "content/shell/shell_content_browser_client.h"
#include "content/shell/shell_content_renderer_client.h"
#include "content/shell/shell_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_MACOSX)
#include "content/shell/paths_mac.h"
#endif  // OS_MACOSX

namespace {

void InitLogging() {
  FilePath log_filename;
  PathService::Get(base::DIR_EXE, &log_filename);
  log_filename = log_filename.AppendASCII("content_shell.log");
  logging::InitLogging(
      log_filename.value().c_str(),
      logging::LOG_ONLY_TO_FILE,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  logging::SetLogItems(true, true, true, true);
}

}  // namespace

ShellMainDelegate::ShellMainDelegate() {
}

ShellMainDelegate::~ShellMainDelegate() {
#if defined(OS_ANDROID)
  NOTREACHED();
#endif
}

bool ShellMainDelegate::BasicStartupComplete(int* exit_code) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    InitLogging();
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAllowFileAccessFromFiles);
  }
#if defined(OS_MACOSX)
  OverrideFrameworkBundlePath();
#endif
  content::SetContentClient(&content_client_);
  return false;
}

void ShellMainDelegate::PreSandboxStartup() {
#if defined(OS_MACOSX)
  OverrideChildProcessPath();
#endif  // OS_MACOSX
  InitializeResourceBundle();
}

int ShellMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (!process_type.empty())
    return -1;

#if !defined(OS_ANDROID)
  return ShellBrowserMain(main_function_params);
#else
  // If no process type is specified, we are creating the main browser process.
  browser_runner_.reset(content::BrowserMainRunner::Create());
  int exit_code = browser_runner_->Initialize(main_function_params);
  DCHECK(exit_code < 0)
      << "BrowserRunner::Initialize failed in ShellMainDelegate";

  return exit_code;
#endif
}

void ShellMainDelegate::InitializeResourceBundle() {
  FilePath pak_file;
#if defined(OS_MACOSX)
  pak_file = GetResourcesPakFilePath();
#else
  FilePath pak_dir;

#if defined(OS_ANDROID)
  DCHECK(PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_dir));
  pak_dir = pak_dir.Append(FILE_PATH_LITERAL("paks"));
#else
  PathService::Get(base::DIR_MODULE, &pak_dir);
#endif

  pak_file = pak_dir.Append(FILE_PATH_LITERAL("content_shell.pak"));
#endif
  ui::ResourceBundle::InitSharedInstanceWithPakFile(pak_file);
}

content::ContentBrowserClient* ShellMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(new content::ShellContentBrowserClient);
  return browser_client_.get();
}

content::ContentRendererClient*
    ShellMainDelegate::CreateContentRendererClient() {
  renderer_client_.reset(new content::ShellContentRendererClient);
  return renderer_client_.get();
}
