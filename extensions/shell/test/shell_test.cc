// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_test.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_system.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_extension_system.h"

namespace extensions {

AppShellTest::AppShellTest()
    : browser_context_(nullptr),
      extension_system_(nullptr) {
#if defined(OS_MACOSX)
  // TODO(phajdan.jr): Make browser tests self-contained on Mac; remove this.
  // Set up the application path as though we we are inside the App Shell.app
  // bundle, rather than the top-level extensions_browsertests, because we
  // make many assumptions about where the executable is located.
  base::FilePath app_shell_path;
  CHECK(PathService::Get(base::FILE_EXE, &app_shell_path));
  app_shell_path = app_shell_path.DirName();
  app_shell_path = app_shell_path.Append(
      FILE_PATH_LITERAL("App Shell.app/Contents/MacOS/App Shell"));
  CHECK(PathService::Override(base::FILE_EXE, app_shell_path));
#endif
}

AppShellTest::~AppShellTest() {
}

void AppShellTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kTestType, "appshell");
  SetUpCommandLine(command_line);
  content::BrowserTestBase::SetUp();
}

void AppShellTest::PreRunTestOnMainThread() {
  browser_context_ = ShellContentBrowserClient::Get()->GetBrowserContext();

  extension_system_ = static_cast<ShellExtensionSystem*>(
      ExtensionSystem::Get(browser_context_));
  extension_system_->Init();
  DCHECK(base::MessageLoopForUI::IsCurrent());
  base::RunLoop().RunUntilIdle();
}

void AppShellTest::PostRunTestOnMainThread() {
  // Clean up the app window.
  DesktopController::instance()->CloseAppWindows();
}

}  // namespace extensions
