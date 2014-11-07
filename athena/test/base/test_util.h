// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_BASE_TEST_UTIL_H_
#define ATHENA_TEST_BASE_TEST_UTIL_H_

#include "athena/resource_manager/public/resource_manager.h"
#include "base/strings/string16.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace athena {
class Activity;

namespace test_util {
// Sends a memory pressure event to the resource manager with a new |pressure|
// level. turning off asynchronous pressure changed events.
void SendTestMemoryPressureEvent(ResourceManager::MemoryPressure pressure);

// Create a new web activity and return after the page is fully loaded.
Activity* CreateTestWebActivity(content::BrowserContext* context,
                                const base::string16& title,
                                const GURL& url);

// Wait until the system is idle.
void WaitUntilIdle();

}  // namespace test_util

}  // namespace athena

#endif  //  ATHENA_TEST_BASE_TEST_UTIL_H_
