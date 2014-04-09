// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/test/shell_test.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension_paths.h"

namespace apps {

// Test that we can open an app window and wait for it to load.
IN_PROC_BROWSER_TEST_F(AppShellTest, Basic) {
  base::FilePath test_data_dir;

  content::WindowedNotificationObserver test_pass_observer(
      chrome::NOTIFICATION_EXTENSION_TEST_PASSED,
      content::NotificationService::AllSources());

  PathService::Get(extensions::DIR_TEST_DATA, &test_data_dir);
  test_data_dir = test_data_dir.AppendASCII("platform_app");
  LoadAndLaunchApp(test_data_dir);

  test_pass_observer.Wait();
}

}  // namespace apps
