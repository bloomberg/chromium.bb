// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NSS_UTIL_H_
#define BASE_NSS_UTIL_H_

#include "base/basictypes.h"

#if defined(USE_NSS)
class Lock;
#endif  // defined(USE_NSS)

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

#if defined(USE_NSS)
// NSS has a bug which can cause a deadlock or stall in some cases when writing
// to the certDB and keyDB. It also has a bug which causes concurrent key pair
// generations to scribble over each other. To work around this, we synchronize
// writes to the NSS databases with a global lock. The lock is hidden beneath a
// function for easy disabling when the bug is fixed. Callers should allow for
// it to return NULL in the future.
//
// See https://bugzilla.mozilla.org/show_bug.cgi?id=564011
Lock* GetNSSWriteLock();

// A helper class that acquires the NSS write Lock while the AutoNSSWriteLock
// is in scope.
class AutoNSSWriteLock {
 public:
  AutoNSSWriteLock();
  ~AutoNSSWriteLock();
 private:
  Lock *lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoNSSWriteLock);
};

#endif  // defined(USE_NSS)

}  // namespace base

#endif  // BASE_NSS_UTIL_H_
