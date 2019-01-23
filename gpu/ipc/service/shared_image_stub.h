// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_
#define GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_

#include "base/trace_event/memory_dump_provider.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "ipc/ipc_listener.h"

namespace gpu {
class SharedContextState;
struct Mailbox;
class GpuChannel;
class SharedImageFactory;

class SharedImageStub : public IPC::Listener,
                        public MemoryTracker,
                        public base::trace_event::MemoryDumpProvider {
 public:
  SharedImageStub(GpuChannel* channel, int32_t route_id);
  ~SharedImageStub() override;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;

  // MemoryTracker implementation:
  void TrackMemoryAllocatedChange(uint64_t delta) override;
  uint64_t GetSize() const override;
  uint64_t ClientTracingId() const override;
  int ClientId() const override;
  uint64_t ContextGroupTracingId() const override;

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  SequenceId sequence() const { return sequence_; }

 private:
  void OnCreateSharedImage(
      const GpuChannelMsg_CreateSharedImage_Params& params);
  void OnCreateSharedImageWithData(
      const GpuChannelMsg_CreateSharedImageWithData_Params& params);
  void OnCreateGMBSharedImage(GpuChannelMsg_CreateGMBSharedImage_Params params);
  void OnUpdateSharedImage(const Mailbox& mailbox, uint32_t release_id);
  void OnDestroySharedImage(const Mailbox& mailbox);
  void OnRegisterSharedImageUploadBuffer(base::ReadOnlySharedMemoryRegion shm);
  bool MakeContextCurrent();
  bool MakeContextCurrentAndCreateFactory();
  void OnError();

  GpuChannel* channel_;
  SequenceId sequence_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<SharedImageFactory> factory_;
  uint64_t size_ = 0;
  // Holds shared memory used in initial data uploads.
  base::ReadOnlySharedMemoryRegion upload_memory_;
  base::ReadOnlySharedMemoryMapping upload_memory_mapping_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_
