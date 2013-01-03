// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CRASH_KEYS_H_
#define CHROME_COMMON_CRASH_KEYS_H_

namespace crash_keys {

// Crash Key Name Constants ////////////////////////////////////////////////////

namespace mac {

// Used to report the first Cocoa/Mac NSException and its backtrace.
extern const char kFirstNSException[];
extern const char kFirstNSExceptionTrace[];

// Used to report the last Cocoa/Mac NSException and its backtrace.
extern const char kLastNSException[];
extern const char kLastNSExceptionTrace[];

// Records the current NSException as it's being created.
extern const char kNSException[];

// In the CrApplication, records information about the current event's
// target-action.
extern const char kSendAction[];

// Records Cocoa zombie/used-after-freed objects that resulted in a
// deliberate crash.
extern const char kZombie[];
extern const char kZombieTrace[];

}  // namespace mac

}  // namespace crash_keys

#endif  // CHROME_COMMON_CRASH_KEYS_H_
