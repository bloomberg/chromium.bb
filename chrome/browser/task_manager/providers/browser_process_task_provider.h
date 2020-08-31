// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_BROWSER_PROCESS_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_BROWSER_PROCESS_TASK_PROVIDER_H_

#include "base/macros.h"
#include "chrome/browser/task_manager/providers/browser_process_task.h"
#include "chrome/browser/task_manager/providers/task_provider.h"

namespace task_manager {

// This provides the browser process task which lives as long as the browser
// lives.
class BrowserProcessTaskProvider : public TaskProvider {
 public:
  BrowserProcessTaskProvider();
  ~BrowserProcessTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

 private:
  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // This is the task that represents the one and only main browser process. It
  // lives as long as the browser lives.
  BrowserProcessTask browser_process_task_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessTaskProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_BROWSER_PROCESS_TASK_PROVIDER_H_
