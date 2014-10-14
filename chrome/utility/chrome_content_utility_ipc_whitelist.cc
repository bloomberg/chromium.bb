// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_ipc_whitelist.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#endif

#if defined(ENABLE_EXTENSIONS)
const uint32 kMessageWhitelist[] = {
#if defined(OS_WIN)
    ChromeUtilityHostMsg_GetWiFiCredentials::ID,
#endif  // defined(OS_WIN)
    ChromeUtilityMsg_ImageWriter_Cancel::ID,
    ChromeUtilityMsg_ImageWriter_Write::ID,
    ChromeUtilityMsg_ImageWriter_Verify::ID
};
const size_t kMessageWhitelistSize = arraysize(kMessageWhitelist);
#else
// Note: Zero-size arrays are not valid C++.
const uint32 kMessageWhitelist[] = { 0 };
const size_t kMessageWhitelistSize = 0;
#endif  // defined(ENABLE_EXTENSIONS)
