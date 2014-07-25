// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_constants.h"

namespace installer {

// Elements that make up install paths.
const wchar_t kChromeArchive[] = L"chrome.7z";
const wchar_t kChromeCompressedArchive[] = L"chrome.packed.7z";
const wchar_t kVisualElements[] = L"VisualElements";
const wchar_t kVisualElementsManifest[] = L"VisualElementsManifest.xml";
const wchar_t kWowHelperExe[] = L"wow_helper.exe";

// Sub directory of install source package under install temporary directory.
const wchar_t kInstallSourceDir[] = L"source";
const wchar_t kInstallSourceChromeDir[] = L"Chrome-bin";

const wchar_t kMediaPlayerRegPath[] =
    L"Software\\Microsoft\\MediaPlayer\\ShimInclusionList";

// Key names for values passed via the GoogleUpdateUntrustedData environment
// variable.
const char kInstallFromWebstore[] = "install-from-webstore";

}  // namespace installer
