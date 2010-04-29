// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NSS_UTIL_H_
#define BASE_NSS_UTIL_H_

#include "base/basictypes.h"

// This file specifically doesn't depend on any NSS or NSPR headers because it
// is included by various (non-crypto) parts of chrome to call the
// initialization functions.
namespace base {

class Time;

// Initialize NRPR if it isn't already initialized.  This function is
// thread-safe, and NSPR will only ever be initialized once.  NSPR will be
// properly shut down on program exit.
void EnsureNSPRInit();

// Initialize NSS if it isn't already initialized.  This must be called before
// any other NSS functions.  This function is thread-safe, and NSS will only
// ever be initialized once.  NSS will be properly shut down on program exit.
void EnsureNSSInit();

#if defined(OS_CHROMEOS)
// Open the r/w nssdb that's stored inside the user's encrypted home directory.
void OpenPersistentNSSDB();
#endif

// Convert a NSS PRTime value into a base::Time object.
// We use a int64 instead of PRTime here to avoid depending on NSPR headers.
Time PRTimeToBaseTime(int64 prtime);

}  // namespace base

#endif  // BASE_NSS_UTIL_H_
