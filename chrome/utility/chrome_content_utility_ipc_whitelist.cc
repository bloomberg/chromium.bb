// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_ipc_whitelist.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#endif

const uint32 kMessageWhitelist[] = {
#if defined(ENABLE_EXTENSIONS)
#if defined(OS_WIN)
    ChromeUtilityHostMsg_GetAndEncryptWiFiCredentials::ID,
#endif  // defined(OS_WIN)
    ChromeUtilityMsg_ImageWriter_Cancel::ID,
    ChromeUtilityMsg_ImageWriter_Write::ID,
    ChromeUtilityMsg_ImageWriter_Verify::ID
#endif  // defined(ENABLE_EXTENSIONS)
};

const size_t kMessageWhitelistSize = ARRAYSIZE_UNSAFE(kMessageWhitelist);
