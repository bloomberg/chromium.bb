// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_gpu_channel_establish_factory.h"

namespace content {

MockGpuChannelEstablishFactory::MockGpuChannelEstablishFactory() = default;

MockGpuChannelEstablishFactory::~MockGpuChannelEstablishFactory() = default;

void MockGpuChannelEstablishFactory::EstablishGpuChannel(
    const gpu::GpuChannelEstablishedCallback& callback) {
  callback.Run(nullptr);
}

scoped_refptr<gpu::GpuChannelHost>
MockGpuChannelEstablishFactory::EstablishGpuChannelSync() {
  return nullptr;
}

gpu::GpuMemoryBufferManager*
MockGpuChannelEstablishFactory::GetGpuMemoryBufferManager() {
  return nullptr;
}

}  // namespace content
