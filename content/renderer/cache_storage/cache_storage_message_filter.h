// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_CACHE_STORAGE_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_CACHE_STORAGE_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/renderer/worker_thread_message_filter.h"

namespace content {

class CacheStorageMessageFilter : public WorkerThreadMessageFilter {
 public:
  CacheStorageMessageFilter(
      ThreadSafeSender* thread_safe_sender,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);

 protected:
  ~CacheStorageMessageFilter() override;

 private:
  // WorkerThreadMessageFilter:
  bool ShouldHandleMessage(const IPC::Message& msg) const override;
  void OnFilteredMessageReceived(const IPC::Message& msg) override;
  bool GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                   int* ipc_thread_id) override;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_CACHE_STORAGE_MESSAGE_FILTER_H_
