// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/master_prefs.h"

#include "base/file_util.h"
#include "base/mac/foundation_util.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_version_info.h"

namespace {

#if defined(GOOGLE_CHROME_BUILD)
// This should be NSApplicationSupportDirectory, but it has already been
// released using NSLibraryDirectory.
const NSSearchPathDirectory kSearchPath = NSLibraryDirectory;
const char kMasterPreferencesDirectory[] = "Google";
const char kMasterPreferencesFileName[] = "Google Chrome Master Preferences";
#else
const NSSearchPathDirectory kSearchPath = NSApplicationSupportDirectory;
const char kMasterPreferencesDirectory[] = "Chromium";
const char kMasterPreferencesFileName[] = "Chromium Master Preferences";
#endif  // GOOGLE_CHROME_BUILD

}  // namespace


namespace master_prefs {

FilePath MasterPrefsPath() {
#if defined(GOOGLE_CHROME_BUILD)
  // Don't load master preferences for the canary.
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_CANARY)
    return FilePath();
#endif  // GOOGLE_CHROME_BUILD

  // On official builds, try
  //~/Library/Application Support/Google/Chrome/Google Chrome Master Preferences
  // On chromium builds, try
  //~/Library/Application Support/Chromium/Chromium Master Preferences
  // This intentionally doesn't use eventual --user-data-dir overrides.
  FilePath user_application_support_path;
  if (chrome::GetDefaultUserDataDirectory(&user_application_support_path)) {
    user_application_support_path =
        user_application_support_path.Append(kMasterPreferencesFileName);
    if (file_util::PathExists(user_application_support_path))
      return user_application_support_path;
  }

  // On official builds, try /Library/Google/Google Chrome Master Preferences
  // On chromium builds, try
  // /Library/Application Support/Chromium/Chromium Master Preferences
  FilePath search_path;
  if (!base::mac::GetLocalDirectory(kSearchPath, &search_path))
    return FilePath();

  return search_path.Append(kMasterPreferencesDirectory)
                    .Append(kMasterPreferencesFileName);
}

}  // namespace master_prefs
