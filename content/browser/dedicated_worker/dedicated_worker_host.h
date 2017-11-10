// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEDICATED_WORKER_DEDICATED_WORKER_HOST_H_
#define CONTENT_BROWSER_DEDICATED_WORKER_DEDICATED_WORKER_HOST_H_

#include "third_party/WebKit/public/platform/dedicated_worker_factory.mojom.h"

namespace url {
class Origin;
}

namespace content {
class RenderProcessHost;

void CreateDedicatedWorkerHostFactory(
    blink::mojom::DedicatedWorkerFactoryRequest request,
    RenderProcessHost* host,
    const url::Origin& origin);

}  // namespace content

#endif  // CONTENT_BROWSER_DEDICATED_WORKER_DEDICATED_WORKER_HOST_H_
