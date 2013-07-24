// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_OS_RESOURCE_WIN_H_
#define CHROME_BROWSER_TASK_MANAGER_OS_RESOURCE_WIN_H_

#include "base/process/process_handle.h"

namespace task_manager {

// Get the current number of GDI handles in use (and peak on >= Win7+).
void GetWinGDIHandles(base::ProcessHandle process,
                      size_t* current,
                      size_t* peak);

// Get the current number of USER handles in use (and peak on >= Win7).
void GetWinUSERHandles(base::ProcessHandle process,
                       size_t* current,
                       size_t* peak);

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_OS_RESOURCE_WIN_H_
