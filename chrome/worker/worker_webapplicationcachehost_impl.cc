// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/worker_webapplicationcachehost_impl.h"

#include "chrome/common/appcache/appcache_dispatcher.h"
#include "chrome/worker/worker_thread.h"

WorkerWebApplicationCacheHostImpl::WorkerWebApplicationCacheHostImpl(
    const WorkerAppCacheInitInfo& init_info,
    WebKit::WebApplicationCacheHostClient* client)
    : WebApplicationCacheHostImpl(client,
          WorkerThread::current()->appcache_dispatcher()->backend_proxy()) {
  if (init_info.is_shared_worker)
    backend()->SelectCacheForSharedWorker(host_id(),
                                          init_info.main_resource_appcache_id);
  else
    backend()->SelectCacheForWorker(host_id(),
                                    init_info.parent_process_id,
                                    init_info.parent_appcache_host_id);
}
