// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/test/shell_test.h"

namespace extensions {

// Test that we can open an app window and wait for it to load.
IN_PROC_BROWSER_TEST_F(AppShellTest, Basic) {
  base::FilePath test_data_dir;

  content::WindowedNotificationObserver test_pass_observer(
      extensions::NOTIFICATION_EXTENSION_TEST_PASSED,
      content::NotificationService::AllSources());

  PathService::Get(extensions::DIR_TEST_DATA, &test_data_dir);
  test_data_dir = test_data_dir.AppendASCII("platform_app");
  LoadAndLaunchApp(test_data_dir);

  test_pass_observer.Wait();
}

}  // namespace extensions
