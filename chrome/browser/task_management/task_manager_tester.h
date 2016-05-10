// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_TESTER_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_TESTER_H_

#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "chrome/browser/task_management/task_manager_browsertest_util.h"

namespace task_management {

// An adapter that abstracts away the difference of old vs. new task manager.
class TaskManagerTester {
 public:
  using ColumnSpecifier = browsertest_util::ColumnSpecifier;

  // Creates a TaskManagerTester backed by the current task manager. The task
  // manager should already be visible when you call this function. |callback|,
  // if not a null callback, will be invoked when the underlying model changes.
  static std::unique_ptr<TaskManagerTester> Create(
      const base::Closure& callback);

  virtual ~TaskManagerTester() {}

  // Get the number of rows currently in the task manager.
  virtual int GetRowCount() = 0;

  // Get the title text of a particular |row|.
  virtual base::string16 GetRowTitle(int row) = 0;

  // Hide or show a column. If a column is not visible its stats are not
  // necessarily gathered.
  virtual void ToggleColumnVisibility(ColumnSpecifier column) = 0;

  // Get the value of a column as an int64. Memory values are in bytes.
  virtual int64_t GetColumnValue(ColumnSpecifier column, int row) = 0;

  // If |row| is associated with a WebContents, return its SessionID. Otherwise,
  // return -1.
  virtual int32_t GetTabId(int row) = 0;

  // Kill the process of |row|.
  virtual void Kill(int row) = 0;
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_TASK_MANAGER_TESTER_H_
