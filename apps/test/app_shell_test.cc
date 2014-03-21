// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/test/app_shell_test.h"

#include "apps/apps_client.h"
#include "apps/shell/browser/shell_browser_context.h"
#include "apps/shell/browser/shell_extension_system.h"
#include "apps/shell/browser/shell_extension_system_factory.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_paths.h"

namespace apps {

AppShellTest::AppShellTest() {}

AppShellTest::~AppShellTest() {}

void AppShellTest::SetUpOnMainThread() {
  std::vector<content::BrowserContext*> contexts =
      AppsClient::Get()->GetLoadedBrowserContexts();
  CHECK_EQ(1U, contexts.size());
  browser_context_ = contexts[0];

  extension_system_ = static_cast<extensions::ShellExtensionSystem*>(
      extensions::ExtensionSystem::Get(browser_context_));
}

void AppShellTest::RunTestOnMainThreadLoop() {
  // Pump startup related events.
  base::MessageLoopForUI::current()->RunUntilIdle();

  SetUpOnMainThread();

  RunTestOnMainThread();

  TearDownOnMainThread();

  // TODO(yoz): Make windows close. This doesn't seem to close the root window.
  extension_system_->CloseApp();
}

bool AppShellTest::LoadAndLaunchApp(const base::FilePath& app_dir) {
  return extension_system_->LoadAndLaunchApp(app_dir);
}

}  // namespace apps
