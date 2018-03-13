// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_TASK_QUEUE_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_TASK_QUEUE_H_

#include "base/callback.h"
#include "base/containers/queue.h"
#include "chromeos/dbus/smb_provider_client.h"

namespace chromeos {
namespace smb_client {

// An SmbTask is a call to SmbProviderClient with a bound SmbFileSystem callback
// that runs when SmbProviderClient receives a D-Bus message response.
using SmbTask = base::OnceClosure;

// SmbTaskQueue handles the queuing of SmbTasks. Tasks are 'pending' while
// SmbProviderClient awaits a D-Bus Message Response. Tasks are added to
// the queue via SmbTaskQueue::AddTask. Upon the SmbFileSystem callback in the
// task running, the caller must call SmbTaskQueue::TaskFinished to allow the
// next task to run.
//
// Example:
//
//  void CreateDirectory(FilePath directory_path, bool recursive,
//                       StatusCallback callback) {
//    auto reply = base::BindOnce(&SmbFileSystem::HandleStatusCallback,
//                                AsWeakPtr(), callback);
//
//    SmbTask task = base::BindOnce(&SmbProviderClient::CreateDirectory,
//                                  base::Unretained(GetSmbProviderClient()),
//                                  GetMountId(),
//                                  directory_path,
//                                  recursive,
//                                  std::move(reply));
//    tq_.AddTask(std::move(task));
//  }
//
//  void HandleStatusCallback(StatusCallback callback, ErrorType error) {
//    tq_.TaskFinished();
//    callback.Run(error);
//  }
class SmbTaskQueue {
 public:
  // Adds |task| to the task queue. If fewer that max_pending_ tasks are
  // outstanding, |task| will run immediately. Otherwise, it will be added to
  // operations_ and run in FIFO order.
  void AddTask(SmbTask task);

  // Must be called by the owner of this class to indicate that a response to a
  // task was received.
  void TaskFinished();

  explicit SmbTaskQueue(size_t max_pending);
  ~SmbTaskQueue();

 private:
  // This runs the next task in operations_ if there is capacity to run an
  // additional task, and a task remaing to run.
  void RunTaskIfNeccessary();

  // Helper method that returns the next task to run.
  SmbTask GetNextTask();

  // Helper method that runs the next task.
  void RunNextTask();

  // Helper method that returns whether there are tasks in operations_ to run.
  bool IsTaskToRun() const;

  // Helper method that returns whether there are fewer than max_pending tasks
  // outstanding.
  bool IsCapacityToRunTask() const;

  const size_t max_pending_;
  size_t num_pending_ = 0;
  base::queue<SmbTask> operations_;

  DISALLOW_COPY_AND_ASSIGN(SmbTaskQueue);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_TASK_QUEUE_H_
