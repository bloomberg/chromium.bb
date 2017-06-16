// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_GPU_CONTENT_GPU_CLIENT_H_
#define CONTENT_PUBLIC_GPU_CONTENT_GPU_CLIENT_H_

#include "base/metrics/field_trial.h"
#include "content/public/common/content_client.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace gpu {
class SyncPointManager;
struct GpuPreferences;
}

namespace content {

// Embedder API for participating in gpu logic.
class CONTENT_EXPORT ContentGpuClient {
 public:
  virtual ~ContentGpuClient() {}

  // Initializes the client. This sets up the field trial synchronization
  // mechanism, which will notify |observer| when a field trial is activated,
  // which should be used to inform the browser process of this state.
  // |registry| will be passed to a ConnectionFilter (which lives on the IO
  // thread). Unlike other childthreads, the client must register additional
  // interfaces on this registry rather than just creating more
  // ConnectionFilters as the ConnectionFilter that wraps this registry
  // specifically does not bind any interface requests until after the Gpu
  // process receives CreateGpuService() from the browser.
  virtual void Initialize(base::FieldTrialList::Observer* observer,
                          service_manager::BinderRegistry* registry) {}

  // Called during initialization once the GpuService has been initialized.
  virtual void GpuServiceInitialized(
      const gpu::GpuPreferences& gpu_preferences) {}

  // Allows client to supply a SyncPointManager instance instead of having
  // content internally create one.
  virtual gpu::SyncPointManager* GetSyncPointManager();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_GPU_CONTENT_GPU_CLIENT_H_
