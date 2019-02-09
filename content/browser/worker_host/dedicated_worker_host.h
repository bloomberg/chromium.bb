// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
#define CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_

#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"

namespace url {
class Origin;
}

namespace content {

// Creates a host factory for a dedicated worker. This must be called on the UI
// thread.
void CreateDedicatedWorkerHostFactory(
    int process_id,
    int parent_render_frame_id,
    const url::Origin& origin,
    blink::mojom::DedicatedWorkerHostFactoryRequest request);

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
