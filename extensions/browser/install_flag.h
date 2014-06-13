// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_FLAG_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_FLAG_H_

namespace extensions {

// Flags used when installing an extension, through ExtensionService and
// ExtensionPrefs and beyond.
enum InstallFlag {
  kInstallFlagNone = 0,

  // The requirements of the extension weren't met (for example graphics
  // capabilities).
  kInstallFlagHasRequirementErrors = 1 << 0,

  // Extension is blacklisted for being malware.
  kInstallFlagIsBlacklistedForMalware = 1 << 1,

  // This is an ephemeral app.
  kInstallFlagIsEphemeral = 1 << 2,

  // Install the extension immediately, don't wait until idle.
  kInstallFlagInstallImmediately = 1 << 3,
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_FLAG_H_
