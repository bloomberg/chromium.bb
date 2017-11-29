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
#include "content/common/quota_dispatcher_host.mojom.h"
#include "content/public/renderer/worker_thread.h"

class GURL;

namespace blink {
class WebStorageQuotaCallbacks;
}

namespace content {

// Dispatches and sends quota related messages sent to/from a child
// process from/to the main browser process.  There is one instance
// per each thread.  Thread-specific instance can be obtained by
// ThreadSpecificInstance().
// TODO(sashab): Change this to be per-execution context instead of per-process.
class QuotaDispatcher : public WorkerThread::Observer {
 public:
  // TODO(sashab): Remove this wrapper, using lambdas or just the web callback
  // itself.
  class Callback {
   public:
    virtual ~Callback() {}
    virtual void DidQueryStorageUsageAndQuota(int64_t usage, int64_t quota) = 0;
    virtual void DidGrantStorageQuota(int64_t usage, int64_t granted_quota) = 0;
    virtual void DidFail(storage::QuotaStatusCode status) = 0;
  };

  explicit QuotaDispatcher(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~QuotaDispatcher() override;

  static QuotaDispatcher* ThreadSpecificInstance(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  void QueryStorageUsageAndQuota(const GURL& gurl,
                                 storage::StorageType type,
                                 std::unique_ptr<Callback> callback);
  void RequestStorageQuota(int render_frame_id,
                           const GURL& gurl,
                           storage::StorageType type,
                           int64_t requested_size,
                           std::unique_ptr<Callback> callback);

  // Creates a new Callback instance for WebStorageQuotaCallbacks.
  static std::unique_ptr<Callback> CreateWebStorageQuotaCallbacksWrapper(
      blink::WebStorageQuotaCallbacks callbacks);

 private:
  // Message handlers.
  void DidQueryStorageUsageAndQuota(int64_t request_id,
                                    storage::QuotaStatusCode status,
                                    int64_t current_usage,
                                    int64_t current_quota);
  void DidGrantStorageQuota(int64_t request_id,
                            storage::QuotaStatusCode status,
                            int64_t current_usage,
                            int64_t granted_quota);
  void DidFail(int request_id, storage::QuotaStatusCode error);

  content::mojom::QuotaDispatcherHostPtr quota_host_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // TODO(sashab, nverne): Once default callbacks are available for dropped mojo
  // callbacks (crbug.com/775358), use them to call DidFail for them in the
  // destructor and remove this.
  base::IDMap<std::unique_ptr<Callback>> pending_quota_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(QuotaDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_QUOTA_DISPATCHER_H_
