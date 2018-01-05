// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_QUOTA_DISPATCHER_H_
#define CONTENT_RENDERER_QUOTA_DISPATCHER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "content/public/renderer/worker_thread.h"
#include "third_party/WebKit/common/quota/quota_dispatcher_host.mojom.h"

namespace url {
class Origin;
}

namespace content {

// Dispatches and sends quota related messages sent to/from a child
// process from/to the main browser process.  There is one instance
// per each thread.  Thread-specific instance can be obtained by
// ThreadSpecificInstance().
// TODO(sashab): Change this to be per-execution context instead of per-process.
class QuotaDispatcher : public WorkerThread::Observer {
 public:
  explicit QuotaDispatcher(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~QuotaDispatcher() override;

  static QuotaDispatcher* ThreadSpecificInstance(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  void QueryStorageUsageAndQuota(
      const url::Origin& origin,
      blink::mojom::StorageType type,
      blink::mojom::QuotaDispatcherHost::QueryStorageUsageAndQuotaCallback);
  void RequestStorageQuota(
      int render_frame_id,
      const url::Origin& origin,
      blink::mojom::StorageType type,
      int64_t requested_size,
      blink::mojom::QuotaDispatcherHost::RequestStorageQuotaCallback);

 private:
  blink::mojom::QuotaDispatcherHostPtr quota_host_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(QuotaDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_QUOTA_DISPATCHER_H_
