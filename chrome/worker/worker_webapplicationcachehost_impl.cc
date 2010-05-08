// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/worker_webapplicationcachehost_impl.h"

#include "base/logging.h"
#include "chrome/common/appcache/appcache_dispatcher.h"
#include "chrome/worker/worker_thread.h"

WorkerWebApplicationCacheHostImpl::WorkerWebApplicationCacheHostImpl(
    const WorkerAppCacheInitInfo& init_info,
    WebKit::WebApplicationCacheHostClient* client)
    : WebApplicationCacheHostImpl(client,
          WorkerThread::current()->appcache_dispatcher()->backend_proxy()) {
  // TODO(michaeln): Send a worker specific init message.
  // backend_->SelectCacheForWorker(init_info);
}
