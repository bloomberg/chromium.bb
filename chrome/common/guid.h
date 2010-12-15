// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GUID_H_
#define CHROME_COMMON_GUID_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "build/build_config.h"

namespace guid {

// Generate a 128-bit random GUID of the form: "%08X-%04X-%04X-%04X-%012llX".
// If GUID generation fails an empty string is returned.
// The POSIX implementation uses psuedo random number generation to create
// the GUID.  The Windows implementation uses system services.
std::string GenerateGUID();

// Returns true if the input string conforms to the GUID format.
bool IsValidGUID(const std::string& guid);

#if defined(OS_POSIX)
// For unit testing purposes only.  Do not use outside of tests.
std::string RandomDataToGUIDString(const uint64 bytes[2]);
#endif

}  // namespace guid

#endif  // CHROME_COMMON_GUID_H_
