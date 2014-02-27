// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_ipc_whitelist.h"
#include "chrome/common/chrome_utility_messages.h"

namespace chrome {

const uint32 kMessageWhitelist[] = {ChromeUtilityMsg_ImageWriter_Cancel::ID,
                                    ChromeUtilityMsg_ImageWriter_Write::ID,
                                    ChromeUtilityMsg_ImageWriter_Verify::ID};
const size_t kMessageWhitelistSize = arraysize(kMessageWhitelist);

}  // namespace chrome
