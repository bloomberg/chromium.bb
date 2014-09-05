// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_test.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_system.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_desktop_controller.h"
#include "extensions/shell/browser/shell_extension_system.h"

namespace extensions {

AppShellTest::AppShellTest() : browser_context_(NULL), extension_system_(NULL) {
}

AppShellTest::~AppShellTest() {
}

void AppShellTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kTestType, "appshell");
  content::BrowserTestBase::SetUp();
}

void AppShellTest::SetUpOnMainThread() {
  browser_context_ = ShellContentBrowserClient::Get()->GetBrowserContext();

  extension_system_ = static_cast<ShellExtensionSystem*>(
      ExtensionSystem::Get(browser_context_));
}

void AppShellTest::RunTestOnMainThreadLoop() {
  base::MessageLoopForUI::current()->RunUntilIdle();

  SetUpOnMainThread();

  RunTestOnMainThread();

  TearDownOnMainThread();

  // Clean up the app window.
  ShellDesktopController::instance()->CloseAppWindows();
}

}  // namespace extensions
