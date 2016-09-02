// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_GPU_CHANNEL_ESTABLISH_FACTORY_H_
#define CONTENT_TEST_MOCK_GPU_CHANNEL_ESTABLISH_FACTORY_H_

#include "base/macros.h"
#include "gpu/ipc/client/gpu_channel_host.h"

namespace content {

class MockGpuChannelEstablishFactory : public gpu::GpuChannelEstablishFactory {
 public:
  MockGpuChannelEstablishFactory();
  ~MockGpuChannelEstablishFactory() override;

  void EstablishGpuChannel(
      const gpu::GpuChannelEstablishedCallback& callback) override;
  scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannelSync() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_GPU_CHANNEL_ESTABLISH_FACTORY_H_
