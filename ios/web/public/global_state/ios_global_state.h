// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_GLOBAL_STATE_IOS_GLOBAL_STATE_H_
#define IOS_WEB_PUBLIC_GLOBAL_STATE_IOS_GLOBAL_STATE_H_

#include "base/task_scheduler/task_scheduler.h"

namespace ios_global_state {

// Creates global state for iOS. This should be called as early as possible in
// the application lifecycle. It is safe to call this method more than once, the
// initialization will only be performed once.
void Create();

// Starts a global base::TaskScheduler. This method must be called to start
// the Task Scheduler that is created in |Create|. If |init_params| is null,
// default InitParams will be used. It is safe to call this method more than
// once, the task scheduler will only be started once.
void StartTaskScheduler(base::TaskScheduler::InitParams* init_params);

}  // namespace ios_global_state

#endif  // IOS_WEB_PUBLIC_GLOBAL_STATE_IOS_GLOBAL_STATE_H_
