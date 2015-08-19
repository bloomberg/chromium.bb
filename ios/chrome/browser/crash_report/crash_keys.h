// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_KEYS_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_KEYS_H_

#include <stddef.h>

// Registers all of the potential crash keys that can be sent to the crash
// reporting server. Returns the size of the union of all keys.
size_t RegisterChromeIOSCrashKeys();

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_KEYS_H_
