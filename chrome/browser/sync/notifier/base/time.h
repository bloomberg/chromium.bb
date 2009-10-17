// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_TIME_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_TIME_H_

#include <time.h>

#include "talk/base/basictypes.h"

typedef uint64 time64;

#define kMicrosecsTo100ns   (static_cast<time64>(10))
#define kMillisecsTo100ns   (static_cast<time64>(10000))
#define kSecsTo100ns        (1000 * kMillisecsTo100ns)
#define kMinsTo100ns        (60 * kSecsTo100ns)
#define kHoursTo100ns       (60 * kMinsTo100ns)
#define kDaysTo100ns        (24 * kHoursTo100ns)
const time64 kMaxTime100ns = UINT64_C(9223372036854775807);

// Time difference in 100NS granularity between platform-dependent starting
// time and Jan 1, 1970.
#if defined(OS_WIN)
// On Windows time64 is seconds since Jan 1, 1601.
#define kStart100NsTimeToEpoch (116444736000000000uI64)
#else
// On Unix time64 is seconds since Jan 1, 1970.
#define kStart100NsTimeToEpoch (0)
#endif

// Time difference in 100NS granularity between platform-dependent starting
// time and Jan 1, 1980.
#define kStart100NsTimeTo1980  \
    kStart100NsTimeToEpoch + UINT64_C(3155328000000000)

#define kTimeGranularity    (kDaysTo100ns)

namespace notifier {

// Get the current time represented in 100NS granularity. Different platform
// might return the value since different starting time. Win32 platform returns
// the value since Jan 1, 1601.
time64 GetCurrent100NSTime();

// Convert from struct tm to time64.
time64 TmToTime64(const struct tm& tm);

// Convert from time64 to struct tm.
bool Time64ToTm(time64 t, struct tm* tm);

// Returns the local time as a string suitable for logging.
// Note: This is *not* threadsafe, so only call it from the main thread.
char* GetLocalTimeAsString();

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_TIME_H_
