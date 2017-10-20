// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TASK_SEND_UNINSTALL_PING_H_
#define COMPONENTS_UPDATE_CLIENT_TASK_SEND_UNINSTALL_PING_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/task.h"
#include "components/update_client/update_client.h"

namespace base {
class Version;
}

namespace update_client {

class UpdateEngine;
enum class Error;

// Defines a specialized task for sending the uninstall ping.
class TaskSendUninstallPing : public Task {
 public:
  using Callback = base::OnceCallback<void(Task* task, Error error)>;

  // |update_engine| is injected here to handle the task.
  // |id| represents the CRX to send the ping for.
  // |callback| is called to return the execution flow back to creator of
  //    this task when the task is done.
  TaskSendUninstallPing(UpdateEngine* update_engine,
                        const std::string& id,
                        const base::Version& version,
                        int reason,
                        Callback callback);
  ~TaskSendUninstallPing() override;

  void Run() override;

  void Cancel() override;

  std::vector<std::string> GetIds() const override;

 private:
  // Called when the task has completed either because the task has run or
  // it has been canceled.
  void TaskComplete(Error error);

  base::ThreadChecker thread_checker_;

  UpdateEngine* update_engine_;  // Not owned by this class.
  const std::string id_;
  const base::Version version_;
  int reason_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(TaskSendUninstallPing);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TASK_SEND_UNINSTALL_PING_H_
