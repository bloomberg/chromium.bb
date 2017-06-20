// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_MANAGER_IO_THREAD_HELPER_H_
#define CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_MANAGER_IO_THREAD_HELPER_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace net {
class URLRequest;
}  // namespace net

namespace task_manager {

// Defines a wrapper of values that will be sent from IO to UI thread upon
// reception and transmission of bytes notifications.
struct BytesTransferredParam {
  // The PID of the originating process of the URLRequest, if the request is
  // sent on behalf of another process. Otherwise it's 0.
  int origin_pid;

  // The unique ID of the host of the child process requester.
  int child_id;

  // The ID of the IPC route for the URLRequest (this identifies the
  // RenderView or like-thing in the renderer that the request gets routed
  // to).
  int route_id;

  // The number of bytes read.
  int64_t byte_read_count;

  // The number of bytes sent.
  int64_t byte_sent_count;

  BytesTransferredParam(int origin_pid,
                        int child_id,
                        int route_id,
                        int64_t byte_read_count,
                        int64_t byte_sent_count)
      : origin_pid(origin_pid),
        child_id(child_id),
        route_id(route_id),
        byte_read_count(byte_read_count),
        byte_sent_count(byte_sent_count) {}
};

// Defines a utility class used to schedule the creation and removal of the
// TaskManagerIoThreadHelper on the IO thread.
class IoThreadHelperManager {
 public:
  IoThreadHelperManager();
  ~IoThreadHelperManager();

 private:
  DISALLOW_COPY_AND_ASSIGN(IoThreadHelperManager);
};

// Defines a class used by the task manager to receive notifications of the
// network bytes transferred by the various tasks.
// This object lives entirely only on the IO thread.
class TaskManagerIoThreadHelper {
 public:
  // Create and delete the instance of this class. They must be called on the IO
  // thread.
  static void CreateInstance();
  static void DeleteInstance();

  // This is used to forward the call to update the network bytes with read
  // bytes from the TaskManagerInterface if the new task manager is enabled.
  static void OnRawBytesRead(const net::URLRequest& request,
                             int64_t bytes_read);

  // This is used to forward the call to update the network bytes with sent
  // bytes from the TaskManagerInterface if the new task manager is enabled.
  static void OnRawBytesSent(const net::URLRequest& request,
                             int64_t bytes_sent);

 private:
  TaskManagerIoThreadHelper();
  ~TaskManagerIoThreadHelper();

  // We gather multiple notifications on the IO thread in one second before a
  // call is made to the following function to start the processing for
  // transferred bytes.
  void OnMultipleBytesTransferredIO();

  // This will update the task manager with the network bytes read.
  void OnNetworkBytesTransferred(const net::URLRequest& request,
                                 int64_t bytes_read,
                                 int64_t bytes_sent);

  // This buffer will be filled on IO thread with information about the number
  // of bytes transferred from URLRequests.
  std::vector<BytesTransferredParam> bytes_transferred_buffer_;

  base::WeakPtrFactory<TaskManagerIoThreadHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerIoThreadHelper);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_MANAGER_IO_THREAD_HELPER_H_
