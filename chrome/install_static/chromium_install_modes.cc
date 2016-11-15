// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific constants and install modes for Chromium.

#include <stdlib.h>

#include "chrome/install_static/install_modes.h"

namespace install_static {

const wchar_t kCompanyPathName[] = L"";

const wchar_t kProductPathName[] = L"Chromium";

const size_t kProductPathNameLength = _countof(kProductPathName) - 1;

// No integration with Google Update, so no app GUID.
const wchar_t kBinariesAppGuid[] = L"";

const wchar_t kBinariesPathName[] = L"Chromium Binaries";

const InstallConstants kInstallModes[] = {
    // The primary (and only) install mode for Chromium.
    {
        sizeof(kInstallModes[0]),
        CHROMIUM_INDEX,
        L"",  // Empty install_suffix for the primary install mode.
        L"",  // Empty app_guid since no integraion with Google Update.
        L"",  // Empty default channel name as above.
        ChannelStrategy::UNSUPPORTED,
        true,  // Supports system-level installs.
        true,  // Supports multi-install.
    },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
