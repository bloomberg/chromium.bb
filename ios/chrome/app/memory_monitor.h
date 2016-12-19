// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MEMORY_MONITOR_H_
#define IOS_CHROME_APP_MEMORY_MONITOR_H_

namespace ios_internal {

// Timer to launch [UpdateBreakpadMemoryValues] every 5 seconds.
void AsynchronousFreeMemoryMonitor();

// Checks the values of free RAM and free disk space and updates breakpad with
// these values.
void UpdateBreakpadMemoryValues();

}  // namespace ios_internal

#endif  // IOS_CHROME_APP_MEMORY_MONITOR_H_
