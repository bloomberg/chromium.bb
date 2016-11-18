// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TASK_SCHEDULER_UTIL_INITIALIZATION_UTIL_H_
#define COMPONENTS_TASK_SCHEDULER_UTIL_INITIALIZATION_UTIL_H_

namespace task_scheduler_util {

// Calls base::TaskScheduler::CreateAndSetDefaultTaskScheduler with arguments
// derived from the variations system or a default known good set of arguments
// if the variations parameters are invalid or missing.
void InitializeDefaultBrowserTaskScheduler();

}  // namespace task_scheduler_util

#endif  // BASE_TASK_SCHEDULER_INITIALIZATION_UTIL_H_
