// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace device {

#if defined(OS_WIN)
// Controls whether on Windows, U2F/CTAP2 requests are forwarded to the
// native WebAuthentication API, where available.
const base::Feature kWebAuthUseNativeWinApi{"WebAuthenticationUseNativeWinApi",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// If true, the minimum API version check for integration with the native
// Windows WebAuthentication API is disabled. This is an interim solution for
// for manual testing while we await the release of a DLL that implements the
// version check.
const base::Feature kWebAuthDisableWinApiVersionCheckForTesting{
    "WebAuthenticationDisableWinApiVersionCheckForTesting",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

extern const base::Feature kWebAuthProxyCryptotoken{
    "WebAuthenticationProxyCryptotoken", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace device
