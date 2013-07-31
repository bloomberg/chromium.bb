// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MAC_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"

#ifdef __OBJC__
@class NSDictionary;
@class NSString;
#else  // __OBJC__
class NSDictionary;
class NSString;
#endif  // __OBJC__

namespace web_app {

// Returns the full path of the .app shim that would be created by
// web_app::CreateShortcuts().
base::FilePath GetAppInstallPath(
    const ShellIntegration::ShortcutInfo& shortcut_info);

// If necessary, launch the shortcut for an app.
void MaybeLaunchShortcut(const ShellIntegration::ShortcutInfo& shortcut_info);

// Creates a shortcut for a web application. The shortcut is a stub app
// that simply loads the browser framework and runs the given app.
class WebAppShortcutCreator {
 public:
  // Creates a new shortcut based on information in |shortcut_info|.
  // A copy of the shortcut is placed in |app_data_path|.
  // |chrome_bundle_id| is the CFBundleIdentifier of the Chrome browser bundle.
  WebAppShortcutCreator(
      const base::FilePath& app_data_path,
      const ShellIntegration::ShortcutInfo& shortcut_info,
      const std::string& chrome_bundle_id);

  virtual ~WebAppShortcutCreator();

  // Returns the base name for the shortcut.
  base::FilePath GetShortcutName() const;

  // Returns a path to the Chrome Apps folder in the relevant applications
  // folder. E.g. ~/Applications or /Applications.
  virtual base::FilePath GetDestinationPath() const;

  bool CreateShortcuts(ShortcutCreationReason creation_reason);
  void DeleteShortcuts();
  bool UpdateShortcuts();

 protected:
  // Returns a path to the app loader.
  base::FilePath GetAppLoaderPath() const;

  // Updates the plist inside |app_path| with information about the app.
  bool UpdatePlist(const base::FilePath& app_path) const;

  // Updates the icon for the shortcut.
  bool UpdateIcon(const base::FilePath& app_path) const;

  // Returns a path to an app bundle with the given id. Or an empty path if no
  // matching bundle was found.
  // Protected and virtual so it can be mocked out for testing.
  virtual base::FilePath GetAppBundleById(const std::string& bundle_id) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, DeleteShortcuts);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, UpdateIcon);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, UpdateShortcuts);

  // Copies the app loader template into a temporary directory and fills in all
  // relevant information.
  bool BuildShortcut(const base::FilePath& staging_path) const;

  // Builds a shortcut and copies it into the given destination folders.
  // Returns with the number of successful copies. Returns on the first failure.
  size_t CreateShortcutsIn(const std::vector<base::FilePath>& folders) const;

  // Updates the InfoPlist.string inside |app_path| with the display name for
  // the app.
  bool UpdateDisplayName(const base::FilePath& app_path) const;

  // Updates the bundle id of the internal copy of the app shim bundle.
  bool UpdateInternalBundleIdentifier() const;

  // Returns the bundle identifier to use for this app bundle.
  std::string GetBundleIdentifier() const;

  // Returns the bundle identifier for the internal copy of the bundle.
  std::string GetInternalBundleIdentifier() const;

  // Show the bundle we just generated in the Finder.
  virtual void RevealAppShimInFinder() const;

  // Path to the data directory for this app. For example:
  // ~/Library/Application Support/Chromium/Default/Web Applications/_crx_abc/
  base::FilePath app_data_path_;

  // Information about the app.
  ShellIntegration::ShortcutInfo info_;

  // The CFBundleIdentifier of the Chrome browser bundle.
  std::string chrome_bundle_id_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MAC_H_
