// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the ChromeELF.

#ifndef CHROME_ELF_CHROME_ELF_CONSTANTS_H_
#define CHROME_ELF_CHROME_ELF_CONSTANTS_H_

// directory names
extern const wchar_t kAppDataDirName[];
extern const wchar_t kCanaryAppDataDirName[];
extern const wchar_t kLocalStateFilename[];
extern const wchar_t kPreferencesFilename[];
extern const wchar_t kUserDataDirName[];

namespace blacklist {

// The registry path of the blacklist beacon.
extern const wchar_t kRegistryBeaconPath[];

// The properties for the blacklist beacon.
extern const wchar_t kBeaconVersion[];
extern const wchar_t kBeaconState[];

// The states for the blacklist setup code.
enum BlacklistState {
  BLACKLIST_DISABLED = 0,
  BLACKLIST_ENABLED,
  // The blacklist setup code is running. If this is still set at startup,
  // it means the last setup crashed.
  BLACKLIST_SETUP_RUNNING,
  // The blacklist thunk setup code is running. If this is still set at startup,
  // it means the last setup crashed during thunk setup.
  BLACKLIST_THUNK_SETUP,
  // The blacklist code is currently intercepting MapViewOfSection. If this is
  // still set at startup, it means we crashed during interception.
  BLACKLIST_INTERCEPTING,
  // Always keep this at the end.
  BLACKLIST_STATE_MAX,
};

}  // namespace blacklist

#endif  // CHROME_ELF_CHROME_ELF_CONSTANTS_H_
