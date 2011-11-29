// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#pragma once

#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/scoped_temp_dir.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_types.h"

class Extension;

// Base class for extension browser tests. Provides utilities for loading,
// unloading, and installing extensions.
class ExtensionBrowserTest
    : public InProcessBrowserTest, public content::NotificationObserver {
 protected:
  ExtensionBrowserTest();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  const Extension* LoadExtension(const FilePath& path);

  // Same as above, but enables the extension in incognito mode first.
  const Extension* LoadExtensionIncognito(const FilePath& path);

  const Extension* LoadExtensionWithOptions(const FilePath& path,
                                            bool incognito_enabled,
                                            bool fileaccess_enabled);

  // Loads extension and imitates that it is a component extension.
  bool LoadExtensionAsComponent(const FilePath& path);

  // Pack the extension in |dir_path| into a crx file and return its path.
  // Return an empty FilePath if there were errors.
  FilePath PackExtension(const FilePath& dir_path);

  // Pack the extension in |dir_path| into a crx file at |crx_path|, using the
  // key |pem_path|. If |pem_path| does not exist, create a new key at
  // |pem_out_path|.
  // Return the path to the crx file, or an empty FilePath if there were errors.
  FilePath PackExtensionWithOptions(const FilePath& dir_path,
                                    const FilePath& crx_path,
                                    const FilePath& pem_path,
                                    const FilePath& pem_out_path);

  // |expected_change| indicates how many extensions should be installed (or
  // disabled, if negative).
  // 1 means you expect a new install, 0 means you expect an upgrade, -1 means
  // you expect a failed upgrade.
  const Extension* InstallExtension(const FilePath& path, int expected_change) {
    return InstallOrUpdateExtension("", path, INSTALL_UI_TYPE_NONE,
                                    expected_change);
  }

  // Installs extension as if it came from the Chrome Webstore.
  const Extension* InstallExtensionFromWebstore(
      const FilePath& path, int expected_change);

  // Same as above but passes an id to CrxInstaller and does not allow a
  // privilege increase.
  const Extension* UpdateExtension(const std::string& id, const FilePath& path,
                                   int expected_change) {
    return InstallOrUpdateExtension(id, path, INSTALL_UI_TYPE_NONE,
                                    expected_change);
  }

  // Same as |InstallExtension| but with the normal extension UI showing up
  // (for e.g. info bar on success).
  const Extension* InstallExtensionWithUI(const FilePath& path,
                                          int expected_change) {
    return InstallOrUpdateExtension("", path, INSTALL_UI_TYPE_NORMAL,
                                    expected_change);
  }
  const Extension* InstallExtensionWithUIAutoConfirm(const FilePath& path,
                                                     int expected_change,
                                                     Profile* profile) {
    return InstallOrUpdateExtension("", path, INSTALL_UI_TYPE_AUTO_CONFIRM,
                                    expected_change, profile, false);
  }

  // Begins install process but simulates a user cancel.
  const Extension* StartInstallButCancel(const FilePath& path) {
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

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  bool loaded_;
  bool installed_;

  // test_data/extensions.
  FilePath test_data_dir_;
  std::string last_loaded_extension_id_;
  int extension_installs_observed_;

 private:
  // Temporary directory for testing.
  ScopedTempDir temp_dir_;

  // Specifies the type of UI (if any) to show during installation and what
  // user action to simulate.
  enum InstallUIType {
    INSTALL_UI_TYPE_NONE,
    INSTALL_UI_TYPE_CANCEL,
    INSTALL_UI_TYPE_NORMAL,
    INSTALL_UI_TYPE_AUTO_CONFIRM,
  };

  const Extension* InstallOrUpdateExtension(const std::string& id,
                                            const FilePath& path,
                                            InstallUIType ui_type,
                                            int expected_change);
  const Extension* InstallOrUpdateExtension(const std::string& id,
                                            const FilePath& path,
                                            InstallUIType ui_type,
                                            int expected_change,
                                            Profile* profile,
                                            bool from_webstore);

  bool WaitForExtensionHostsToLoad();

  // When waiting for page action count to change, we wait until it reaches this
  // value.
  int target_page_action_count_;

  // When waiting for visible page action count to change, we wait until it
  // reaches this value.
  int target_visible_page_action_count_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
