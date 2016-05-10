// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_LEGACY_TASK_MANAGER_TESTER_H_
#define CHROME_BROWSER_TASK_MANAGER_LEGACY_TASK_MANAGER_TESTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "chrome/browser/task_management/task_manager_tester.h"

namespace task_manager {

// Create a LegacyTaskManagerTester for the old (deprecated, mac-only) task
// manager. If you're writing a test, don't use this directly since it won't
// work on other platforms. Use TaskManagerTester::Create() instead.
std::unique_ptr<task_management::TaskManagerTester>
CreateLegacyTaskManagerTester(const base::Closure& callback);

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_LEGACY_TASK_MANAGER_TESTER_H_
