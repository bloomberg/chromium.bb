// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/test/shell_test.h"

#include "apps/shell/browser/shell_content_browser_client.h"
#include "apps/shell/browser/shell_desktop_controller.h"
#include "apps/shell/browser/shell_extension_system.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_system.h"

namespace apps {

AppShellTest::AppShellTest()
    : browser_context_(NULL), extension_system_(NULL) {}

AppShellTest::~AppShellTest() {}

void AppShellTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kTestType, "appshell");
  content::BrowserTestBase::SetUp();
}

void AppShellTest::SetUpOnMainThread() {
  browser_context_ = ShellContentBrowserClient::Get()->GetBrowserContext();

  extension_system_ = static_cast<extensions::ShellExtensionSystem*>(
      extensions::ExtensionSystem::Get(browser_context_));
}

void AppShellTest::RunTestOnMainThreadLoop() {
  base::MessageLoopForUI::current()->RunUntilIdle();

  SetUpOnMainThread();

  RunTestOnMainThread();

  TearDownOnMainThread();

  // Clean up the app window.
  ShellDesktopController::instance()->CloseAppWindows();
}

bool AppShellTest::LoadAndLaunchApp(const base::FilePath& app_dir) {
  bool loaded = extension_system_->LoadApp(app_dir);
  if (loaded)
    extension_system_->LaunchApp();
  return loaded;
}

}  // namespace apps
