// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CEEE module-wide utilities.

#ifndef CEEE_IE_COMMON_TOOLBAND_MODULE_UTIL_H__
#define CEEE_IE_COMMON_TOOLBAND_MODULE_UTIL_H__

#include <wtypes.h>
#include <string>

#include "base/file_path.h"
#include "base/time.h"

namespace ceee_module_util {

void Lock();
void Unlock();

LONG LockModule();
LONG UnlockModule();

// Un/Register a hook to be unhooked by our termination safety net.
// This is to protect ourselves against cases where the hook owner never
// gets torn down (for whatever reason) and then the hook may be called when
// we don't expect it to be.
void RegisterHookForSafetyNet(HHOOK hook);
void UnregisterHookForSafetyNet(HHOOK hook);

class AutoLock {
 public:
  AutoLock() {
    Lock();
  }
  ~AutoLock() {
    Unlock();
  }
};

// Gets the path of a .crx that should be installed at the same time as
// CEEE is installed.  May be empty, meaning no .crx files need to be
// installed.
//
// If the returned path does not have a .crx extension, it should be
// assumed to be an exploded extension directory rather than a .crx
std::wstring GetExtensionPath();

// Return the path and time of the last installation that occurred.
FilePath GetInstalledExtensionPath();
base::Time GetInstalledExtensionTime();

// Return true if extension installation should be attempted according
// to the registry. Note that Chrome will silently fail installation of
// an extension version older than what is already installed.
bool NeedToInstallExtension();

// Sets the right registry values to cache the fact that we asked
// Chrome to install an extension.
void SetInstalledExtensionPath(const FilePath& path);

// Returns true if the given path ends with .crx or is empty (as an
// unset "what to load/install" preference means we should expect
// an extension to already be installed).
bool IsCrxOrEmpty(const std::wstring& path);

// Stores/reads a registry entry that tracks whether the user has made the
// toolband visible or hidden.
void SetOptionToolbandIsHidden(bool isHidden);
bool GetOptionToolbandIsHidden();

// Stores/reads a registry entry that tracks whether intial (after setup)
// positioning of the toolband has been completed.
void SetOptionToolbandForceReposition(bool reposition_next_time);
bool GetOptionToolbandForceReposition();

// Indicates whether ShowDW calls should affect registry tracking of the
// user's visibility preference.
void SetIgnoreShowDWChanges(bool ignore);
bool GetIgnoreShowDWChanges();

// Chooses between kChromeProfileName and kChromeProfileNameForAdmin depending
// on process properties (run as admin or not).
const wchar_t* GetBrokerProfileNameForIe();

// Returns true if Chrome Frame is allowed to send usage stats.
bool GetCollectStatsConsent();

// Returns Google Update ClientState registry key for ChromeFrame.
std::wstring GetCromeFrameClientStateKey();

// The name of the profile that is used by the Firefox CEEE.
extern const wchar_t kChromeProfileNameForFirefox[];

// The name of the Internet Explorer executable.
extern const wchar_t kInternetExplorerModuleName[];

// The name of the CEEE Broker executable.
extern const wchar_t kCeeeBrokerModuleName[];

}  // namespace

#endif  // CEEE_IE_COMMON_TOOLBAND_MODULE_UTIL_H__
