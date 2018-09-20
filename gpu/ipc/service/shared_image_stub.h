// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_
#define GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_

#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "ipc/ipc_listener.h"

struct GpuChannelMsg_CreateSharedImage_Params;

namespace gpu {
struct Mailbox;
class GpuChannel;
class SharedImageFactory;

namespace raster {
struct RasterDecoderContextState;
}

class SharedImageStub : public IPC::Listener, public gles2::MemoryTracker {
 public:
  SharedImageStub(GpuChannel* channel, int32_t route_id);
  ~SharedImageStub() override;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;

  // gles2::MemoryTracker implementation;
  void TrackMemoryAllocatedChange(uint64_t delta) override;
  uint64_t GetSize() const override;
  uint64_t ClientTracingId() const override;
  int ClientId() const override;
  uint64_t ContextGroupTracingId() const override;

  SequenceId sequence() const { return sequence_; }

 private:
  void OnCreateSharedImage(
      const GpuChannelMsg_CreateSharedImage_Params& params);
  void OnDestroySharedImage(const Mailbox& mailbox);
  bool MakeContextCurrentAndCreateFactory();
  void OnError();

  GpuChannel* channel_;
  SequenceId sequence_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  scoped_refptr<raster::RasterDecoderContextState> context_state_;
  std::unique_ptr<SharedImageFactory> factory_;
  uint64_t size_ = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_
