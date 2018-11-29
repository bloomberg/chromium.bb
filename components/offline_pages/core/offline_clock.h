// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_CLOCK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_CLOCK_H_

namespace base {
class Clock;
}

namespace offline_pages {

// Returns the clock to be used for obtaining the current time. This function
// can be called from any threads.
base::Clock* OfflineClock();

// Allows tests to override the clock returned by |OfflineClock()|. For safety,
// use |TestScopedOfflineClock| instead if possible.
void SetOfflineClockForTesting(base::Clock* clock);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_CLOCK_H_
