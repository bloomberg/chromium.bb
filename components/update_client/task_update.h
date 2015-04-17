// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TASK_UPDATE_H_
#define COMPONENTS_UPDATE_CLIENT_TASK_UPDATE_H_

#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/task.h"
#include "components/update_client/update_client.h"

namespace update_client {

class UpdateEngine;

// Defines a specialized task for updating a group of CRXs.
class TaskUpdate : public Task {
 public:
  TaskUpdate(UpdateEngine* update_engine,
             const std::vector<std::string>& ids,
             const UpdateClient::CrxDataCallback& crx_data_callback);
  ~TaskUpdate() override;

  void Run(const Callback& callback) override;

 private:
  // Called when the Run function associated with this task has completed.
  void RunComplete(int error);

  base::ThreadChecker thread_checker_;

  UpdateEngine* update_engine_;  // Not owned by this class.

  const std::vector<std::string> ids_;
  const UpdateClient::CrxDataCallback crx_data_callback_;

  // Called to return the execution flow back to the caller of the
  // Run function when this task has completed .
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(TaskUpdate);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TASK_UPDATE_H_
