// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/master_prefs.h"

#include "base/mac/foundation_util.h"
#include "chrome/common/chrome_version_info.h"

#if defined(GOOGLE_CHROME_BUILD)

namespace {

const char kGoogleDirectory[] = "Google";
const char kMasterPreferencesFileName[] = "Google Chrome Master Preferences";

}  // namespace

#endif  // GOOGLE_CHROME_BUILD

namespace master_prefs {

FilePath MasterPrefsPath() {
#if defined(GOOGLE_CHROME_BUILD)
  // There is only a system-level master preferences file, and only for the
  // official build, and not for the canary.
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();

  if (channel == chrome::VersionInfo::CHANNEL_CANARY)
    return FilePath();

  FilePath library_path;
  if (!base::mac::GetLocalDirectory(NSLibraryDirectory, &library_path))
    return FilePath();

  return library_path.Append(kGoogleDirectory)
                     .Append(kMasterPreferencesFileName);
#else
  return FilePath();
#endif  // GOOGLE_CHROME_BUILD
}

}  // namespace master_prefs
