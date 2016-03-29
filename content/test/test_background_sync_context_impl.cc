// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_background_sync_context_impl.h"

#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/test/test_background_sync_manager.h"

namespace content {

void TestBackgroundSyncContextImpl::CreateBackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!background_sync_manager());

  TestBackgroundSyncManager* manager = new TestBackgroundSyncManager(context);
  set_background_sync_manager_for_testing(
      make_scoped_ptr<BackgroundSyncManager>(manager));
}

}  // namespace content
