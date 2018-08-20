// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_GPU_CLIENT_DELEGATE_H_
#define CONTENT_BROWSER_GPU_BROWSER_GPU_CLIENT_DELEGATE_H_

#include "components/viz/host/gpu_client_delegate.h"

namespace content {

class BrowserGpuClientDelegate : public viz::GpuClientDelegate {
 public:
  BrowserGpuClientDelegate();
  ~BrowserGpuClientDelegate() override;

  // GpuClientDelegate:
  viz::mojom::GpuService* EnsureGpuService() override;
  void EstablishGpuChannel(int client_id,
                           uint64_t client_tracing_id,
                           EstablishGpuChannelCallback callback) override;
  viz::HostGpuMemoryBufferManager* GetGpuMemoryBufferManager() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserGpuClientDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_GPU_CLIENT_DELEGATE_H_
