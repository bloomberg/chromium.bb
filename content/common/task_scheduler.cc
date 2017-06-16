// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/task_scheduler.h"

#include "base/sys_info.h"

namespace content {

int GetMinThreadsInRendererTaskSchedulerForegroundPool() {
  return base::SysInfo::NumberOfProcessors();
}

}  // namespace content
