// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_THREADED_DATA_PROVIDER_H_
#define CONTENT_CHILD_THREADED_DATA_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_channel.h"
#include "ipc/message_filter.h"

struct ResourceMsg_RequestCompleteData;

namespace blink {
class WebThreadedDataReceiver;
}

namespace IPC {
class SyncChannel;
}

namespace scheduler {
class WebThreadImplForWorkerScheduler;
}

namespace content {
class ResourceDispatcher;

class ThreadedDataProvider {
 public:
  ThreadedDataProvider(
      int request_id,
      blink::WebThreadedDataReceiver* threaded_data_receiver,
      linked_ptr<base::SharedMemory> shm_buffer,
      int shm_size,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_);

  // Any destruction of this class has to bounce via the background thread to
  // ensure all data is flushed; call Stop() to start this process.
  void Stop();
  void OnReceivedDataOnBackgroundThread(int data_offset,
                                        int data_length,
                                        int encoded_data_length);

  void OnReceivedDataOnForegroundThread(const char* data,
                                        int data_length,
                                        int encoded_data_length);

  void OnResourceMessageFilterAddedMainThread();
  void OnRequestCompleteForegroundThread(
      base::WeakPtr<ResourceDispatcher> resource_dispatcher,
      const ResourceMsg_RequestCompleteData& request_complete_data,
      const base::TimeTicks& renderer_completion_time);

 private:
  ~ThreadedDataProvider();
  void DestructOnMainThread();

  void StopOnBackgroundThread();
  void OnResourceMessageFilterAddedBackgroundThread();
  void OnRequestCompleteBackgroundThread(
      base::WeakPtr<ResourceDispatcher> resource_dispatcher,
      const ResourceMsg_RequestCompleteData& request_complete_data,
      const base::TimeTicks& renderer_completion_time);
  void ForwardAndACKData(const char* data,
                         int data_length,
                         int encoded_data_length);
  void DataNotifyForegroundThread(
      scoped_ptr<std::vector<char> > data_copy,
      int data_length,
      int encoded_data_length);

  scoped_refptr<IPC::MessageFilter> filter_;
  int request_id_;
  linked_ptr<base::SharedMemory> shm_buffer_;
  int shm_size_;
  scoped_ptr<base::WeakPtrFactory<ThreadedDataProvider> >
      background_thread_weak_factory_;
  scheduler::WebThreadImplForWorkerScheduler& background_thread_;
  IPC::SyncChannel* ipc_channel_;
  blink::WebThreadedDataReceiver* threaded_data_receiver_;
  bool resource_filter_active_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  struct QueuedSharedMemoryData {
    const char* data;
    int length;
    int encoded_length;
  };
  std::vector<QueuedSharedMemoryData> queued_data_;

  base::WeakPtrFactory<ThreadedDataProvider>
      main_thread_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadedDataProvider);
};

}  // namespace content

#endif  // CONTENT_CHILD_THREADED_DATA_PROVIDER_H_
