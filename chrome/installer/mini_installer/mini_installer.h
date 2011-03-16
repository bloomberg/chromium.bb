// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_H_
#define CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_H_
#pragma once

namespace mini_installer {

// Various filenames
const wchar_t kSetupName[] = L"setup.exe";
const wchar_t kChromePrefix[] = L"chrome";
const wchar_t kSetupPrefix[] = L"setup";

// setup.exe command line arguments
const wchar_t kCmdInstallArchive[] = L" --install-archive";
const wchar_t kCmdUpdateSetupExe[] = L" --update-setup-exe";
const wchar_t kCmdNewSetupExe[] = L" --new-setup-exe";

// Command line arguments specific only to the mini installer.
// Note that these constants differ from the kCmdXxx constants above in that
// they do not have leading whitespace.
// Pass --cleanup to the mini installer to delete temporary directories that
// might be left over from previous installation and then exit (i.e. do not
// extract and run setup.exe).
const wchar_t kMiniCmdCleanup[] = L"--cleanup";

// Temp directory prefix that this process creates
const wchar_t kTempPrefix[] = L"CR_";
// Google Update will use the full installer if this suffix is found in the ap
// value.
const wchar_t kFullInstallerSuffix[] = L"-full";
// Google Update will use the single-install configuration if this suffix is
// found in the ap value.
const wchar_t kMultifailInstallerSuffix[] = L"-multifail";
// ap value tag for a multi-install product.
const wchar_t kMultiInstallTag[] = L"-multi";

// The resource types that would be unpacked from the mini installer.
// 'BN' is uncompressed binary and 'BL' is LZ compressed binary.
const wchar_t kBinResourceType[] = L"BN";
const wchar_t kLZCResourceType[] = L"BL";
const wchar_t kLZMAResourceType[] = L"B7";

// Registry key to get uninstall command
const wchar_t kApRegistryValueName[] = L"ap";
// Registry key that tells Chrome installer not to delete extracted files.
const wchar_t kCleanupRegistryValueName[] = L"ChromeInstallerCleanup";
// Registry key to get uninstall command
const wchar_t kUninstallRegistryValueName[] = L"UninstallString";

// Paths for the above registry keys
#if defined(GOOGLE_CHROME_BUILD)
// The concatenation of this plus the Google Update GUID is the app registry
// key.
const wchar_t kApRegistryKeyBase[] = L"Software\\Google\\Update\\ClientState\\";
const wchar_t kUninstallRegistryKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Google Chrome";
const wchar_t kCleanupRegistryKey[] = L"Software\\Google";
#else
const wchar_t kApRegistryKeyBase[] = L"Software\\Chromium";
const wchar_t kUninstallRegistryKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Chromium";
const wchar_t kCleanupRegistryKey[] = L"Software\\Chromium";
#endif

// One gigabyte is the biggest resource size that it can handle.
const int kMaxResourceSize = 1024*1024*1024;

// This is the file that contains the list of files to be linked in the
// executable. This file is updated by the installer generator tool chain.
const wchar_t kManifestFilename[] = L"packed_files.txt";

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_H_

