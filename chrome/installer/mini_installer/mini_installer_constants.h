// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_CONSTANTS_H_
#define CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_CONSTANTS_H_

namespace mini_installer {

// Various filenames and prefixes.
extern const wchar_t kSetupExe[];
extern const wchar_t kChromeArchivePrefix[];
extern const wchar_t kSetupPrefix[];

// Unprefixed command line switch names for setup.exe.
extern const wchar_t kCmdInstallArchive[];
extern const wchar_t kCmdUpdateSetupExe[];
extern const wchar_t kCmdNewSetupExe[];

extern const wchar_t kTempPrefix[];
extern const wchar_t kFullInstallerSuffix[];
extern const wchar_t kMultiInstallTag[];

// The resource types that would be unpacked from the mini installer.
extern const wchar_t kBinResourceType[];
extern const wchar_t kLZCResourceType[];
extern const wchar_t kLZMAResourceType[];

// Registry value names.
extern const wchar_t kApRegistryValue[];
extern const wchar_t kCleanupRegistryValue[];
extern const wchar_t kUninstallRegistryValue[];

// Registry key paths.
extern const wchar_t kClientStateKeyBase[];
extern const wchar_t kCleanupRegistryKey[];

extern const size_t kMaxResourceSize;

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_CONSTANTS_H_
