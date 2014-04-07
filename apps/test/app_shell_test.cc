// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/test/app_shell_test.h"

#include "apps/shell/browser/shell_content_browser_client.h"
#include "apps/shell/browser/shell_extension_system.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "extensions/browser/extension_system.h"

namespace apps {

AppShellTest::AppShellTest()
    : browser_context_(NULL), extension_system_(NULL) {}

AppShellTest::~AppShellTest() {}

void AppShellTest::SetUpOnMainThread() {
  browser_context_ = ShellContentBrowserClient::Get()->GetBrowserContext();

  extension_system_ = static_cast<extensions::ShellExtensionSystem*>(
      extensions::ExtensionSystem::Get(browser_context_));
}

void AppShellTest::RunTestOnMainThreadLoop() {
  // Pump startup related events.
  base::MessageLoopForUI::current()->RunUntilIdle();

  SetUpOnMainThread();

  RunTestOnMainThread();

  TearDownOnMainThread();
}

bool AppShellTest::LoadAndLaunchApp(const base::FilePath& app_dir) {
  return extension_system_->LoadAndLaunchApp(app_dir);
}

}  // namespace apps
