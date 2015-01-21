// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_apitest.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

ShellApiTest::ShellApiTest() {
}

ShellApiTest::~ShellApiTest() {
}

const Extension* ShellApiTest::LoadApp(const std::string& app_dir) {
  base::FilePath test_data_dir;
  PathService::Get(extensions::DIR_TEST_DATA, &test_data_dir);
  test_data_dir = test_data_dir.AppendASCII(app_dir);

  const Extension* extension = extension_system_->LoadApp(test_data_dir);
  if (!extension)
    return NULL;

  extension_system_->LaunchApp(extension->id());

  return extension;
}

bool ShellApiTest::RunAppTest(const std::string& app_dir) {
  ResultCatcher catcher;

  const Extension* extension = LoadApp(app_dir);
  if (!extension)
    return false;

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    return false;
  }

  return true;
}

void ShellApiTest::UnloadApp(const Extension* app) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  ASSERT_TRUE(registry->RemoveEnabled(app->id()));

  UnloadedExtensionInfo::Reason reason(UnloadedExtensionInfo::REASON_DISABLE);
  registry->TriggerOnUnloaded(app, reason);

  // The following notifications are deprecated and in the future, classes
  // should only be observing the ExtensionRegistry.
  UnloadedExtensionInfo details(app, reason);
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
      content::Source<content::BrowserContext>(browser_context()),
      content::Details<UnloadedExtensionInfo>(&details));

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_REMOVED,
      content::Source<content::BrowserContext>(browser_context()),
      content::Details<const Extension>(app));
}

}  // namespace extensions
