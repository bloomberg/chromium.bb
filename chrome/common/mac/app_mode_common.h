// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_APP_MODE_COMMON_H_
#define CHROME_COMMON_MAC_APP_MODE_COMMON_H_

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/string16.h"

// This file contains constants, interfaces, etc. which are common to the
// browser application and the app mode loader (a.k.a. shim).

namespace app_mode {

// Keys for a custom 'launch platform app' Apple event. When Chrome receives
// this event, it should launch the app specified by the direct key in the
// event, in the profile specified by the 'pdir' key.
const AEEventClass kAEChromeAppClass = 'cApp';
const AEEventID kAEChromeAppLaunch = 'lnch';
const AEKeyword kAEProfileDirKey = 'pdir';

// The key under which the browser's bundle ID will be stored in the
// app mode launcher bundle's Info.plist.
extern NSString* const kBrowserBundleIDKey;

// Key for the shortcut ID.
extern NSString* const kCrAppModeShortcutIDKey;

// Key for the app's name.
extern NSString* const kCrAppModeShortcutNameKey;

// Key for the app's URL.
extern NSString* const kCrAppModeShortcutURLKey;

// Key for the app user data directory.
extern NSString* const kCrAppModeUserDataDirKey;

// Key for the app's extension path.
extern NSString* const kCrAppModeProfileDirKey;

// When the Chrome browser is run, it stores its location in the defaults
// system using this key.
extern NSString* const kLastRunAppBundlePathPrefsKey;

// Placeholders used in the app mode loader bundle' Info.plist:
extern NSString* const kShortcutIdPlaceholder; // Extension shortcut ID.
extern NSString* const kShortcutNamePlaceholder; // Extension name.
extern NSString* const kShortcutURLPlaceholder;
// Bundle ID of the Chrome browser bundle.
extern NSString* const kShortcutBrowserBundleIDPlaceholder;

// Current major/minor version numbers of |ChromeAppModeInfo| (defined below).
const unsigned kCurrentChromeAppModeInfoMajorVersion = 1;
const unsigned kCurrentChromeAppModeInfoMinorVersion = 0;

// The structure used to pass information from the app mode loader to the
// (browser) framework. This is versioned using major and minor version numbers,
// written below as v<major>.<minor>. Version-number checking is done by the
// framework, and the framework must accept all structures with the same major
// version number. It may refuse to load if the major version of the structure
// is different from the one it accepts.
struct ChromeAppModeInfo {
 public:
  ChromeAppModeInfo();
  ~ChromeAppModeInfo();

  // Major and minor version number of this structure.
  unsigned major_version;  // Required: all versions
  unsigned minor_version;  // Required: all versions

  // Original |argc| and |argv|.
  int argc;  // Required: v1.0
  char** argv;  // Required: v1.0

  // Versioned path to the browser which is being loaded.
  base::FilePath chrome_versioned_path;  // Required: v1.0

  // Path to Chrome app bundle.
  base::FilePath chrome_outer_bundle_path;  // Required: v1.0

  // Information about the App Mode shortcut:

  // Path to the App Mode Loader application bundle that launched the process.
  base::FilePath app_mode_bundle_path;  // Optional: v1.0

  // Short ID string, preferably derived from |app_mode_short_name|. Should be
  // safe for the file system.
  std::string app_mode_id;  // Required: v1.0

  // Unrestricted (e.g., several-word) UTF8-encoded name for the shortcut.
  string16 app_mode_name;  // Optional: v1.0

  // URL for the shortcut. Must be a valid URL.
  std::string app_mode_url;  // Required: v1.0

  // Path to the app's user data directory.
  base::FilePath user_data_dir;

  // Directory of the profile associated with the app.
  base::FilePath profile_dir;
};

}  // namespace app_mode

#endif  // CHROME_COMMON_MAC_APP_MODE_COMMON_H_
