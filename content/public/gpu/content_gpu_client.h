// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_GPU_CONTENT_GPU_CLIENT_H_
#define CONTENT_PUBLIC_GPU_CONTENT_GPU_CLIENT_H_

#include "content/public/common/content_client.h"

namespace gpu {
class SyncPointManager;
}

namespace content {

class ServiceRegistry;

// Embedder API for participating in gpu logic.
class CONTENT_EXPORT ContentGpuClient {
 public:
  virtual ~ContentGpuClient() {}

  // Allows client to register Mojo services in |registry| on the GPU process.
  // The registered services will be exposed to the browser process through
  // GpuProcessHost.
  virtual void RegisterMojoServices(ServiceRegistry* registry) {}

  // Allows client to supply a SyncPointManager instance instead of having
  // content internally create one.
  virtual gpu::SyncPointManager* GetSyncPointManager();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_GPU_CONTENT_GPU_CLIENT_H_
