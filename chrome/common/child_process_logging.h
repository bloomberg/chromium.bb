// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_LOGGING_H_
#define CHROME_COMMON_CHILD_PROCESS_LOGGING_H_

#include <string>

#include "base/basictypes.h"
#include "base/debug/crash_logging.h"

namespace child_process_logging {

#if defined(OS_POSIX) && !defined(OS_MACOSX)
// These are declared here so the crash reporter can access them directly in
// compromised context without going through the standard library.
extern char g_client_id[];

// Assume command line switches are less than 64 chars.
static const size_t kSwitchLen = 64;
#endif

// Sets the Client ID that is used as GUID if a Chrome process crashes.
void SetClientId(const std::string& client_id);

// Gets the Client ID to be used as GUID for crash reporting. Returns the client
// id in |client_id| if it's known, an empty string otherwise.
std::string GetClientId();

#if defined(OS_WIN)
// Sets up the base/debug/crash_logging.h mechanism.
void Init();
#endif  // defined(OS_WIN)

}  // namespace child_process_logging

#endif  // CHROME_COMMON_CHILD_PROCESS_LOGGING_H_
