// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific constants and install modes for Google Chrome.

#include <stdlib.h>

#include "chrome/install_static/install_modes.h"

namespace install_static {

const wchar_t kCompanyPathName[] = L"Google";

const wchar_t kProductPathName[] = L"Chrome";

const size_t kProductPathNameLength = _countof(kProductPathName) - 1;

const wchar_t kBinariesAppGuid[] = L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

// Google Chrome integrates with Google Update, so the app GUID above is used.
const wchar_t kBinariesPathName[] = L"";

const InstallConstants kInstallModes[] = {
    // The primary install mode for stable Google Chrome.
    {
        sizeof(kInstallModes[0]),
        STABLE_INDEX,
        L"",  // Empty install_suffix for the primary install mode.
        L"{8A69D345-D564-463c-AFF1-A69D9E530F96}",
        L"",  // The empty string means "stable".
        ChannelStrategy::ADDITIONAL_PARAMETERS,
        true,  // Supports system-level installs.
        true,  // Supports multi-install.
    },
    {
        sizeof(kInstallModes[0]),
        CANARY_INDEX,
        L" SxS",
        L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}",
        L"canary",
        ChannelStrategy::FIXED,
        false,  // Does not support system-level installs.
        false,  // Does not support multi-install.
    },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
