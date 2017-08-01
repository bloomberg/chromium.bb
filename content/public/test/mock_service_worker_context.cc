// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_service_worker_context.h"

#include "base/callback.h"
#include "base/logging.h"

namespace content {

MockServiceWorkerContext::MockServiceWorkerContext() {}
MockServiceWorkerContext::~MockServiceWorkerContext() {}

void MockServiceWorkerContext::StartActiveWorkerForPattern(
    const GURL& pattern,
    ServiceWorkerContext::StartActiveWorkerCallback info_callback,
    base::OnceClosure failure_callback) {
  NOTREACHED() << "No mock for StartActiveWorkerForPattern";
}

}  // namespace content
