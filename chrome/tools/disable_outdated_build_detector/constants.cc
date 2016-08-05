// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/disable_outdated_build_detector/constants.h"

namespace switches {

const char kMultiInstall[] = "multi-install";
const char kSystemLevel[] = "system-level";

}  // namespace switches

namespace env {

// The presence of this environment variable with a value of 1 implies that the
// tool should run as a system installation regardless of what is on the command
// line.
const char kGoogleUpdateIsMachine[] = "GoogleUpdateIsMachine";

}  // namespace env

// App GUIDs for Google Chrome and the Google Chrome binaries.
#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kChromeAppGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kBinariesAppGuid[] = L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";
#else
const wchar_t kChromeAppGuid[] = L"";
const wchar_t kBinariesAppGuid[] = L"";
#endif

// The new brand to which organic installs will be switched.
const wchar_t kAOHY[] = L"AOHY";

// The names of registry values within a product's ClientState key.
const wchar_t kBrand[] = L"brand";
const wchar_t kInstallerResult[] = L"InstallerResult";
const wchar_t kInstallerError[] = L"InstallerError";
const wchar_t kInstallerExtraCode1[] = L"InstallerExtraCode1";
const wchar_t kUninstallArguments[] = L"UninstallArguments";
