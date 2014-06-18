// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_CHROME_CONTENT_UTILITY_IPC_WHITELIST_H_
#define CHROME_UTILITY_CHROME_CONTENT_UTILITY_IPC_WHITELIST_H_

#include "base/basictypes.h"

// This array contains the list of IPC messages that the utility process will
// accept when running with elevated privileges.  When new messages need to run
// with elevated privileges, add them here and be sure to add a security
// reviewer.
extern const uint32 kMessageWhitelist[];
extern const size_t kMessageWhitelistSize;

#endif  // CHROME_UTILITY_CHROME_CONTENT_UTILITY_IPC_WHITELIST_H_
