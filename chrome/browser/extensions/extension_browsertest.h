// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"

// Base class for extension browser tests. Provides utilities for loading,
// unloading, and installing extensions.
class ExtensionBrowserTest
    : public InProcessBrowserTest, public NotificationObserver {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line);
  bool LoadExtension(const FilePath& path);
  // |expected_change| indicates how many extensions should be installed (or
  // disabled, if negative).
  // 1 means you expect a new install, 0 means you expect an upgrade, -1 means
  // you expect a failed upgrade.
  bool InstallExtension(const FilePath& path, int expected_change);
  void UninstallExtension(const std::string& extension_id);

  // Wait for the number of visible page actions to change to |count|.
  bool WaitForPageActionVisibilityChangeTo(int count);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  bool loaded_;
  bool installed_;
  FilePath test_data_dir_;

 private:
  bool WaitForExtensionHostsToLoad();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
