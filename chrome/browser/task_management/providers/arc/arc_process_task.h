// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_ARC_ARC_PROCESS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_ARC_ARC_PROCESS_TASK_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/task_management/providers/task.h"

namespace task_management {

// Defines a task that represents an ARC process.
class ArcProcessTask : public Task {
 public:
  ArcProcessTask(
      base::ProcessId pid,
      base::ProcessId nspid,
      const std::string& process_name);
  ~ArcProcessTask() override;

  // task_management::Task:
  Type GetType() const override;
  int GetChildProcessUniqueID() const override;

  base::ProcessId nspid() const { return nspid_; }
  const std::string& process_name() const { return process_name_; }

 private:
  const base::ProcessId nspid_;
  const std::string process_name_;

  DISALLOW_COPY_AND_ASSIGN(ArcProcessTask);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_ARC_ARC_PROCESS_TASK_H_
