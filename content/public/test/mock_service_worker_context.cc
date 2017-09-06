// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_service_worker_context.h"

#include "base/callback.h"
#include "base/logging.h"

namespace content {

MockServiceWorkerContext::MockServiceWorkerContext() {}
MockServiceWorkerContext::~MockServiceWorkerContext() {}

void MockServiceWorkerContext::RegisterServiceWorker(const GURL& pattern,
                                                     const GURL& script_url,
                                                     ResultCallback callback) {
  NOTREACHED();
}
void MockServiceWorkerContext::UnregisterServiceWorker(
    const GURL& pattern,
    ResultCallback callback) {
  NOTREACHED();
}
void MockServiceWorkerContext::CountExternalRequestsForTest(
    const GURL& url,
    const CountExternalRequestsCallback& callback) {
  NOTREACHED();
}

void MockServiceWorkerContext::DeleteForOrigin(const GURL& origin,
                                               ResultCallback callback) {
  NOTREACHED();
}

void MockServiceWorkerContext::StartActiveWorkerForPattern(
    const GURL& pattern,
    ServiceWorkerContext::StartActiveWorkerCallback info_callback,
    base::OnceClosure failure_callback) {
  NOTREACHED();
}

}  // namespace content
