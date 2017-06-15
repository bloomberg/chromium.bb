// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome installer.

#ifndef CHROME_INSTALLER_SETUP_SETUP_CONSTANTS_H__
#define CHROME_INSTALLER_SETUP_SETUP_CONSTANTS_H__

namespace installer {

extern const wchar_t kChromeArchive[];
extern const wchar_t kChromeCompressedArchive[];
extern const wchar_t kVisualElements[];
extern const wchar_t kVisualElementsManifest[];

extern const wchar_t kInstallSourceDir[];
extern const wchar_t kInstallSourceChromeDir[];

extern const wchar_t kMediaPlayerRegPath[];

namespace switches {

extern const char kDelay[];
extern const char kSetDisplayVersionProduct[];
extern const char kSetDisplayVersionValue[];
extern const char kUserExperiment[];

}  // namespace switches

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_SETUP_CONSTANTS_H__
