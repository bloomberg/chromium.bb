// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#pragma once

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"

// Base class for extension browser tests. Provides utilities for loading,
// unloading, and installing extensions.
class ExtensionBrowserTest
    : public InProcessBrowserTest, public NotificationObserver {
 protected:
  ExtensionBrowserTest();

  virtual void SetUpCommandLine(CommandLine* command_line);
  bool LoadExtension(const FilePath& path);

  // Same as above, but enables the extension in incognito mode first.
  bool LoadExtensionIncognito(const FilePath& path);

  // Pack the extension in |dir_path| into a crx file and return its path.
  // Return an empty FilePath if there were errors.
  FilePath PackExtension(const FilePath& dir_path);

  // |expected_change| indicates how many extensions should be installed (or
  // disabled, if negative).
  // 1 means you expect a new install, 0 means you expect an upgrade, -1 means
  // you expect a failed upgrade.
  bool InstallExtension(const FilePath& path, int expected_change) {
    return InstallOrUpdateExtension("", path, INSTALL_UI_TYPE_NONE,
                                    expected_change);
  }

  // Same as above but passes an id to CrxInstaller and does not allow a
  // privilege increase.
  bool UpdateExtension(const std::string& id, const FilePath& path,
                       int expected_change) {
    return InstallOrUpdateExtension(id, path, INSTALL_UI_TYPE_NONE,
                                    expected_change);
  }

  // Same as |InstallExtension| but with the normal extension UI showing up
  // (for e.g. info bar on success).
  bool InstallExtensionWithUI(const FilePath& path, int expected_change) {
    return InstallOrUpdateExtension("", path, INSTALL_UI_TYPE_NORMAL,
                                    expected_change);
  }
  bool InstallExtensionWithUIAutoConfirm(const FilePath& path,
                                         int expected_change,
                                         Profile* profile) {
    return InstallOrUpdateExtension("", path, INSTALL_UI_TYPE_AUTO_CONFIRM,
                                    expected_change, profile);
  }

  // Begins install process but simulates a user cancel.
  bool StartInstallButCancel(const FilePath& path) {
    return InstallOrUpdateExtension("", path, INSTALL_UI_TYPE_CANCEL, 0);
  }

  void ReloadExtension(const std::string& extension_id);

  void UnloadExtension(const std::string& extension_id);

  void UninstallExtension(const std::string& extension_id);

  void DisableExtension(const std::string& extension_id);

  void EnableExtension(const std::string& extension_id);

  // Wait for the total number of page actions to change to |count|.
  bool WaitForPageActionCountChangeTo(int count);

  // Wait for the number of visible page actions to change to |count|.
  bool WaitForPageActionVisibilityChangeTo(int count);

  // Waits until an extension is installed and loaded. Returns true if an
  // install happened before timeout.
  bool WaitForExtensionInstall();

  // Wait for an extension install error to be raised. Returns true if an
  // error was raised.
  bool WaitForExtensionInstallError();

  // Waits until an extension is loaded.
  void WaitForExtensionLoad();

  // Wait for the specified extension to crash. Returns true if it really
  // crashed.
  bool WaitForExtensionCrash(const std::string& extension_id);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  bool loaded_;
  bool installed_;

  // test_data/extensions.
  FilePath test_data_dir_;
  std::string last_loaded_extension_id_;
  int extension_installs_observed_;

 private:
  // Specifies the type of UI (if any) to show during installation and what
  // user action to simulate.
  enum InstallUIType {
    INSTALL_UI_TYPE_NONE,
    INSTALL_UI_TYPE_CANCEL,
    INSTALL_UI_TYPE_NORMAL,
    INSTALL_UI_TYPE_AUTO_CONFIRM,
  };

  bool InstallOrUpdateExtension(const std::string& id, const FilePath& path,
                                InstallUIType ui_type,
                                int expected_change);
  bool InstallOrUpdateExtension(const std::string& id, const FilePath& path,
                                InstallUIType ui_type,
                                int expected_change,
                                Profile* profile);
  bool LoadExtensionImpl(const FilePath& path, bool incognito_enabled);

  bool WaitForExtensionHostsToLoad();

  // When waiting for page action count to change, we wait until it reaches this
  // value.
  int target_page_action_count_;

  // When waiting for visible page action count to change, we wait until it
  // reaches this value.
  int target_visible_page_action_count_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
