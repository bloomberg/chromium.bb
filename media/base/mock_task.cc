// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_task.h"

namespace media {

TaskMocker::TaskMocker()
    : outstanding_tasks_(0) {
}

TaskMocker::~TaskMocker() {
  CHECK(outstanding_tasks_ == 0)
      << "If outstanding_tasks_ is not zero, tasks have been leaked.";
}

Task* TaskMocker::CreateTask() {
  return new CountingTask(this);
}

}  // namespace media
