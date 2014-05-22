// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

#include <string>

#include "base/command_line.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/extensions/extension_test_notification_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"

class ExtensionService;
class Profile;

namespace extensions {
class ExtensionCacheFake;
class ExtensionSet;
class ProcessManager;
}

// Base class for extension browser tests. Provides utilities for loading,
// unloading, and installing extensions.
class ExtensionBrowserTest : virtual public InProcessBrowserTest {
 protected:
  // Flags used to configure how the tests are run.
  enum Flags {
    kFlagNone = 0,

    // Allow the extension to run in incognito mode.
    kFlagEnableIncognito = 1 << 0,

    // Allow file access for the extension.
    kFlagEnableFileAccess = 1 << 1,

    // Don't fail when the loaded manifest has warnings (should only be used
    // when testing deprecated features).
    kFlagIgnoreManifestWarnings = 1 << 2,

    // Allow older manifest versions (typically these can't be loaded - we allow
    // them for testing).
    kFlagAllowOldManifestVersions = 1 << 3,
  };

  ExtensionBrowserTest();
  virtual ~ExtensionBrowserTest();

  // Useful accessors.
  ExtensionService* extension_service() {
    return extensions::ExtensionSystem::Get(profile())->extension_service();
  }

  const std::string& last_loaded_extension_id() {
    return observer_->last_loaded_extension_id();
  }

  // Get the profile to use.
  virtual Profile* profile();

  static const extensions::Extension* GetExtensionByPath(
      const extensions::ExtensionSet* extensions, const base::FilePath& path);

  // InProcessBrowserTest
  virtual void SetUp() OVERRIDE;
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

  const extensions::Extension* LoadExtension(const base::FilePath& path);

  // Load extension and enable it in incognito mode.
  const extensions::Extension* LoadExtensionIncognito(
      const base::FilePath& path);

  // Load extension from the |path| folder. |flags| is bit mask of values from
  // |Flags| enum.
  const extensions::Extension* LoadExtensionWithFlags(
      const base::FilePath& path, int flags);

  // Same as above, but sets the installation parameter to the extension
  // preferences.
  const extensions::Extension* LoadExtensionWithInstallParam(
      const base::FilePath& path,
      int flags,
      const std::string& install_param);

  // Loads unpacked extension from |path| with manifest |manifest_relative_path|
  // and imitates that it is a component extension.
  // |manifest_relative_path| is relative to |path|.
  const extensions::Extension* LoadExtensionAsComponentWithManifest(
      const base::FilePath& path,
      const base::FilePath::CharType* manifest_relative_path);

  // Loads unpacked extension from |path| and imitates that it is a component
  // extension. Equivalent to
  // LoadExtensionAsComponentWithManifest(path, extensions::kManifestFilename).
  const extensions::Extension* LoadExtensionAsComponent(
      const base::FilePath& path);

  // Pack the extension in |dir_path| into a crx file and return its path.
  // Return an empty FilePath if there were errors.
  base::FilePath PackExtension(const base::FilePath& dir_path);

  // Pack the extension in |dir_path| into a crx file at |crx_path|, using the
  // key |pem_path|. If |pem_path| does not exist, create a new key at
  // |pem_out_path|.
  // Return the path to the crx file, or an empty FilePath if there were errors.
  base::FilePath PackExtensionWithOptions(const base::FilePath& dir_path,
                                          const base::FilePath& crx_path,
                                          const base::FilePath& pem_path,
                                          const base::FilePath& pem_out_path);

  // |expected_change| indicates how many extensions should be installed (or
  // disabled, if negative).
  // 1 means you expect a new install, 0 means you expect an upgrade, -1 means
  // you expect a failed upgrade.
  const extensions::Extension* InstallExtension(const base::FilePath& path,
                                                int expected_change) {
    return InstallOrUpdateExtension(
        std::string(), path, INSTALL_UI_TYPE_NONE, expected_change);
  }

  // Same as above, but an install source other than Manifest::INTERNAL can be
  // specified.
  const extensions::Extension* InstallExtension(
      const base::FilePath& path,
      int expected_change,
      extensions::Manifest::Location install_source) {
    return InstallOrUpdateExtension(std::string(),
                                    path,
                                    INSTALL_UI_TYPE_NONE,
                                    expected_change,
                                    install_source);
  }

  // Installs extension as if it came from the Chrome Webstore.
  const extensions::Extension* InstallExtensionFromWebstore(
      const base::FilePath& path, int expected_change);

  // Same as above but passes an id to CrxInstaller and does not allow a
  // privilege increase.
  const extensions::Extension* UpdateExtension(const std::string& id,
                                               const base::FilePath& path,
                                               int expected_change) {
    return InstallOrUpdateExtension(id, path, INSTALL_UI_TYPE_NONE,
                                    expected_change);
  }

  // Same as UpdateExtension but waits for the extension to be idle first.
  const extensions::Extension* UpdateExtensionWaitForIdle(
      const std::string& id, const base::FilePath& path, int expected_change);

  // Same as |InstallExtension| but with the normal extension UI showing up
  // (for e.g. info bar on success).
  const extensions::Extension* InstallExtensionWithUI(
      const base::FilePath& path,
      int expected_change) {
    return InstallOrUpdateExtension(
        std::string(), path, INSTALL_UI_TYPE_NORMAL, expected_change);
  }

  const extensions::Extension* InstallExtensionWithUIAutoConfirm(
      const base::FilePath& path,
      int expected_change,
      Browser* browser) {
    return InstallOrUpdateExtension(std::string(),
                                    path,
                                    INSTALL_UI_TYPE_AUTO_CONFIRM,
                                    expected_change,
                                    browser,
                                    extensions::Extension::NO_FLAGS);
  }

  const extensions::Extension* InstallExtensionWithSourceAndFlags(
      const base::FilePath& path,
      int expected_change,
      extensions::Manifest::Location install_source,
      extensions::Extension::InitFromValueFlags creation_flags) {
    return InstallOrUpdateExtension(std::string(),
                                    path,
                                    INSTALL_UI_TYPE_NONE,
                                    expected_change,
                                    install_source,
                                    browser(),
                                    creation_flags,
                                    false,
                                    false);
  }

  const extensions::Extension* InstallEphemeralAppWithSourceAndFlags(
      const base::FilePath& path,
      int expected_change,
      extensions::Manifest::Location install_source,
      extensions::Extension::InitFromValueFlags creation_flags) {
    return InstallOrUpdateExtension(std::string(),
                                    path,
                                    INSTALL_UI_TYPE_NONE,
                                    expected_change,
                                    install_source,
                                    browser(),
                                    creation_flags,
                                    false,
                                    true);
  }

  // Begins install process but simulates a user cancel.
  const extensions::Extension* StartInstallButCancel(
      const base::FilePath& path) {
    return InstallOrUpdateExtension(
        std::string(), path, INSTALL_UI_TYPE_CANCEL, 0);
  }

  void ReloadExtension(const std::string extension_id);

  void UnloadExtension(const std::string& extension_id);

  void UninstallExtension(const std::string& extension_id);

  void DisableExtension(const std::string& extension_id);

  void EnableExtension(const std::string& extension_id);

  // Wait for the total number of page actions to change to |count|.
  bool WaitForPageActionCountChangeTo(int count) {
    return observer_->WaitForPageActionCountChangeTo(count);
  }

  // Wait for the number of visible page actions to change to |count|.
  bool WaitForPageActionVisibilityChangeTo(int count) {
    return observer_->WaitForPageActionVisibilityChangeTo(count);
  }

  // Waits until an extension is installed and loaded. Returns true if an
  // install happened before timeout.
  bool WaitForExtensionInstall() {
    return observer_->WaitForExtensionInstall();
  }

  // Wait for an extension install error to be raised. Returns true if an
  // error was raised.
  bool WaitForExtensionInstallError() {
    return observer_->WaitForExtensionInstallError();
  }

  // Waits until an extension is loaded and all view have loaded.
  void WaitForExtensionAndViewLoad() {
    return observer_->WaitForExtensionAndViewLoad();
  }

  // Waits until an extension is loaded.
  void WaitForExtensionLoad() {
    return observer_->WaitForExtensionLoad();
  }

  // Waits for an extension load error. Returns true if the error really
  // happened.
  bool WaitForExtensionLoadError() {
    return observer_->WaitForExtensionLoadError();
  }

  // Wait for the specified extension to crash. Returns true if it really
  // crashed.
  bool WaitForExtensionCrash(const std::string& extension_id) {
    return observer_->WaitForExtensionCrash(extension_id);
  }

  // Wait for the crx installer to be done. Returns true if it really is done.
  bool WaitForCrxInstallerDone() {
    return observer_->WaitForCrxInstallerDone();
  }

  // Wait for all extension views to load.
  bool WaitForExtensionViewsToLoad() {
    return observer_->WaitForExtensionViewsToLoad();
  }

  // Simulates a page calling window.open on an URL and waits for the
  // navigation.
  void OpenWindow(content::WebContents* contents,
                  const GURL& url,
                  bool newtab_process_should_equal_opener,
                  content::WebContents** newtab_result);

  // Simulates a page navigating itself to an URL and waits for the
  // navigation.
  void NavigateInRenderer(content::WebContents* contents, const GURL& url);

  // Looks for an ExtensionHost whose URL has the given path component
  // (including leading slash).  Also verifies that the expected number of hosts
  // are loaded.
  extensions::ExtensionHost* FindHostWithPath(
      extensions::ProcessManager* manager,
      const std::string& path,
      int expected_hosts);

  // Returns
  // extensions::browsertest_util::ExecuteScriptInBackgroundPage(profile(),
  // extension_id, script).
  std::string ExecuteScriptInBackgroundPage(const std::string& extension_id,
                                            const std::string& script);

  // Returns
  // extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
  // profile(), extension_id, script).
  bool ExecuteScriptInBackgroundPageNoWait(const std::string& extension_id,
                                           const std::string& script);

  bool loaded_;
  bool installed_;

#if defined(OS_CHROMEOS)
  // True if the command line should be tweaked as if ChromeOS user is
  // already logged in.
  bool set_chromeos_user_;
#endif

  // test_data/extensions.
  base::FilePath test_data_dir_;

  scoped_ptr<ExtensionTestNotificationObserver> observer_;

 private:
  // Temporary directory for testing.
  base::ScopedTempDir temp_dir_;

  // Specifies the type of UI (if any) to show during installation and what
  // user action to simulate.
  enum InstallUIType {
    INSTALL_UI_TYPE_NONE,
    INSTALL_UI_TYPE_CANCEL,
    INSTALL_UI_TYPE_NORMAL,
    INSTALL_UI_TYPE_AUTO_CONFIRM,
  };

  const extensions::Extension* InstallOrUpdateExtension(
      const std::string& id,
      const base::FilePath& path,
      InstallUIType ui_type,
      int expected_change);
  const extensions::Extension* InstallOrUpdateExtension(
      const std::string& id,
      const base::FilePath& path,
      InstallUIType ui_type,
      int expected_change,
      Browser* browser,
      extensions::Extension::InitFromValueFlags creation_flags);
  const extensions::Extension* InstallOrUpdateExtension(
      const std::string& id,
      const base::FilePath& path,
      InstallUIType ui_type,
      int expected_change,
      extensions::Manifest::Location install_source);
  const extensions::Extension* InstallOrUpdateExtension(
      const std::string& id,
      const base::FilePath& path,
      InstallUIType ui_type,
      int expected_change,
      extensions::Manifest::Location install_source,
      Browser* browser,
      extensions::Extension::InitFromValueFlags creation_flags,
      bool wait_for_idle,
      bool is_ephemeral);

  // Make the current channel "dev" for the duration of the test.
  extensions::ScopedCurrentChannel current_channel_;

  // Disable external install UI.
  extensions::FeatureSwitch::ScopedOverride
      override_prompt_for_external_extensions_;

  // The default profile to be used.
  Profile* profile_;

  // Cache cache implementation.
  scoped_ptr<extensions::ExtensionCacheFake> test_extension_cache_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
