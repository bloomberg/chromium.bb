// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/multi_process_notification.h"

#include "base/task.h"

namespace multi_process_notification {

PerformTaskOnNotification::PerformTaskOnNotification(Task* task) : task_(task) {
}

PerformTaskOnNotification::~PerformTaskOnNotification() {
}

void PerformTaskOnNotification::OnNotificationReceived(
    const std::string& name, Domain domain) {
  task_->Run();
  task_.reset();
}

bool PerformTaskOnNotification::WasNotificationReceived() {
  return task_.get() == NULL;
}

}  // namespace multi_process_notification
