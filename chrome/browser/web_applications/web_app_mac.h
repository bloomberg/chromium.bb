// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MAC_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/shell_integration.h"

#ifdef __OBJC__
@class NSDictionary;
@class NSString;
#else  // __OBJC__
class NSDictionary;
class NSString;
#endif  // __OBJC__

namespace web_app {

// Creates a shortcut for a web application. The shortcut is a stub app
// that simply loads the browser framework and runs the given app.
class WebAppShortcutCreator {
 public:
  // Creates a new shortcut based on information in |shortcut_info|.
  // The shortcut stores its user data directory in |user_data_dir|.
  // |chrome_bundle_id| is the CFBundleIdentifier of the Chrome browser bundle.
  WebAppShortcutCreator(
      const base::FilePath& user_data_dir,
      const ShellIntegration::ShortcutInfo& shortcut_info,
      const string16& chrome_bundle_id);

  virtual ~WebAppShortcutCreator();

  // Copies the app launcher template into place and fills in all relevant
  // information.
  bool CreateShortcut();

 protected:
  // Returns a path to the app loader.
  base::FilePath GetAppLoaderPath() const;

  // Returns a path to the destination where the app should be written to.
  virtual base::FilePath GetDestinationPath() const;

  // Updates the plist inside |app_path| with information about the app.
  bool UpdatePlist(const base::FilePath& app_path) const;

  // Updates the icon for the shortcut.
  bool UpdateIcon(const base::FilePath& app_path) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, UpdateIcon);

  // Path to the app's user data directory. For example:
  // ~/Library/Application Support/Chromium/Default/Web Applications/_crx_abc/
  // Note, the user data directory is the parent of the profile directory.
  base::FilePath user_data_dir_;

  // Returns the bundle identifier to use for this app bundle.
  // |plist| is a dictionary containg a copy of the template plist file to
  // be used for creating the app bundle.
  NSString* GetBundleIdentifier(NSDictionary* plist) const;

  // Show the bundle we just generated in the Finder.
  virtual void RevealGeneratedBundleInFinder(
      const base::FilePath& generated_bundle) const;

  // Information about the app.
  ShellIntegration::ShortcutInfo info_;

  // The CFBundleIdentifier of the Chrome browser bundle.
  string16 chrome_bundle_id_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MAC_H_
