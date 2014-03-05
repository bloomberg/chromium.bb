// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_

#include "base/strings/string16.h"

namespace task_manager {
namespace browsertest_util {

// Runs the message loop, observing the task manager, until there are exactly
// |resource_count| many resources whose titles match the pattern
// |title_pattern|. The match is done via string_util's base::MatchPattern, so
// |title_pattern| may contain wildcards like "*".
//
// If the wait times out, this test will trigger a gtest failure. To get
// meaningful errors, tests should wrap invocations of this function with
// ASSERT_NO_FATAL_FAILURE().
void WaitForTaskManagerRows(int resource_count,
                            const base::string16& title_pattern);

}  // namespace browsertest_util
}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_
