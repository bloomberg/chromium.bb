// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_COMMAND_BUFFER_STUB_H_
#define GPU_IPC_SERVICE_GPU_COMMAND_BUFFER_STUB_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_memory_manager.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"
#include "ui/latency/latency_info.h"
#include "url/gurl.h"

struct GPUCreateCommandBufferConfig;
struct GpuCommandBufferMsg_CreateImage_Params;

namespace gl {
class GLShareGroup;
}

namespace gpu {
struct Mailbox;
struct SyncToken;
struct WaitForCommandState;
class GpuChannel;
class SyncPointClientState;

class GPU_EXPORT GpuCommandBufferStub
    : public IPC::Listener,
      public IPC::Sender,
      public CommandBufferServiceClient,
      public gles2::GLES2DecoderClient,
      public ImageTransportSurfaceDelegate,
      public base::SupportsWeakPtr<GpuCommandBufferStub> {
 public:
  class DestructionObserver {
   public:
    // Called in Destroy(), before the context/surface are released.
    virtual void OnWillDestroyStub() = 0;

   protected:
    virtual ~DestructionObserver() {}
  };

  typedef base::Callback<void(const std::vector<ui::LatencyInfo>&)>
      LatencyInfoCallback;

  GpuCommandBufferStub(GpuChannel* channel,
                       const GPUCreateCommandBufferConfig& init_params,
                       CommandBufferId command_buffer_id,
                       SequenceId sequence_id,
                       int32_t stream_id,
                       int32_t route_id);

  ~GpuCommandBufferStub() override;

  // This must leave the GL context associated with the newly-created
  // GpuCommandBufferStub current, so the GpuChannel can initialize
  // the gpu::Capabilities.
  gpu::ContextResult Initialize(
      GpuCommandBufferStub* share_group,
      const GPUCreateCommandBufferConfig& init_params,
      std::unique_ptr<base::SharedMemory> shared_state_shm);

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // CommandBufferServiceClient implementation:
  CommandBatchProcessedResult OnCommandBatchProcessed() override;
  void OnParseError() override;

  // GLES2DecoderClient implementation:
  void OnConsoleMessage(int32_t id, const std::string& message) override;
  void CacheShader(const std::string& key, const std::string& shader) override;
  void OnFenceSyncRelease(uint64_t release) override;
  bool OnWaitSyncToken(const SyncToken& sync_token) override;
  void OnDescheduleUntilFinished() override;
  void OnRescheduleAfterFinished() override;

// ImageTransportSurfaceDelegate implementation:
#if defined(OS_WIN)
  void DidCreateAcceleratedSurfaceChildWindow(
      SurfaceHandle parent_window,
      SurfaceHandle child_window) override;
#endif
  void DidSwapBuffersComplete(SwapBuffersCompleteParams params) override;
  const gles2::FeatureInfo* GetFeatureInfo() const override;
  const GpuPreferences& GetGpuPreferences() const override;
  void SetLatencyInfoCallback(const LatencyInfoCallback& callback) override;
  void UpdateVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;

  void AddFilter(IPC::MessageFilter* message_filter) override;
  int32_t GetRouteID() const override;

  gles2::MemoryTracker* GetMemoryTracker() const;

  // Whether this command buffer can currently handle IPC messages.
  bool IsScheduled();

  // Whether there are commands in the buffer that haven't been processed.
  bool HasUnprocessedCommands();

  gles2::GLES2Decoder* decoder() const { return decoder_.get(); }
  GpuChannel* channel() const { return channel_; }

  // Unique command buffer ID for this command buffer stub.
  CommandBufferId command_buffer_id() const { return command_buffer_id_; }

  SequenceId sequence_id() const { return sequence_id_; }

  int32_t stream_id() const { return stream_id_; }

  gl::GLSurface* surface() const { return surface_.get(); }

  void AddDestructionObserver(DestructionObserver* observer);
  void RemoveDestructionObserver(DestructionObserver* observer);

  void MarkContextLost();

 private:
  GpuMemoryManager* GetMemoryManager() const;

  void Destroy();

  bool MakeCurrent();

  // Message handlers:
  void OnSetGetBuffer(int32_t shm_id);
  void OnTakeFrontBuffer(const Mailbox& mailbox);
  void OnReturnFrontBuffer(const Mailbox& mailbox, bool is_lost);
  void OnGetState(IPC::Message* reply_message);
  void OnWaitForTokenInRange(int32_t start,
                             int32_t end,
                             IPC::Message* reply_message);
  void OnWaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                 int32_t start,
                                 int32_t end,
                                 IPC::Message* reply_message);
  void OnAsyncFlush(int32_t put_offset,
                    uint32_t flush_id,
                    const std::vector<ui::LatencyInfo>& latency_info);
  void OnRegisterTransferBuffer(int32_t id,
                                base::SharedMemoryHandle transfer_buffer,
                                uint32_t size);
  void OnDestroyTransferBuffer(int32_t id);
  void OnGetTransferBuffer(int32_t id, IPC::Message* reply_message);

  void OnEnsureBackbuffer();

  void OnSignalSyncToken(const SyncToken& sync_token, uint32_t id);
  void OnSignalAck(uint32_t id);
  void OnSignalQuery(uint32_t query, uint32_t id);

  void OnWaitSyncTokenCompleted(const SyncToken& sync_token);

  void OnCreateImage(const GpuCommandBufferMsg_CreateImage_Params& params);
  void OnDestroyImage(int32_t id);
  void OnCreateStreamTexture(uint32_t texture_id,
                             int32_t stream_id,
                             bool* succeeded);

  void ReportState();

  // Poll the command buffer to execute work.
  void PollWork();
  void PerformWork();

  // Schedule processing of delayed work. This updates the time at which
  // delayed work should be processed. |process_delayed_work_time_| is
  // updated to current time + delay. Call this after processing some amount
  // of delayed work.
  void ScheduleDelayedWork(base::TimeDelta delay);

  bool CheckContextLost();
  void CheckCompleteWaits();

  // Set driver bug workarounds and disabled GL extensions to the context.
  static void SetContextGpuFeatureInfo(gl::GLContext* context,
                                       const GpuFeatureInfo& gpu_feature_info);

  // The lifetime of objects of this class is managed by a GpuChannel. The
  // GpuChannels destroy all the GpuCommandBufferStubs that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannel* const channel_;

  // The group of contexts that share namespaces with this context.
  scoped_refptr<gles2::ContextGroup> context_group_;

  bool initialized_;
  const SurfaceHandle surface_handle_;
  bool use_virtualized_gl_context_;
  const CommandBufferId command_buffer_id_;
  const SequenceId sequence_id_;
  const int32_t stream_id_;
  const int32_t route_id_;
  uint32_t last_flush_id_;

  std::unique_ptr<CommandBufferService> command_buffer_;
  std::unique_ptr<gles2::GLES2Decoder> decoder_;
  scoped_refptr<SyncPointClientState> sync_point_client_state_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLShareGroup> share_group_;

  base::ObserverList<DestructionObserver> destruction_observers_;

  bool waiting_for_sync_point_;

  base::TimeTicks process_delayed_work_time_;
  uint32_t previous_processed_num_;
  base::TimeTicks last_idle_time_;

  LatencyInfoCallback latency_info_callback_;

  GURL active_url_;
  size_t active_url_hash_;

  std::unique_ptr<WaitForCommandState> wait_for_token_;
  std::unique_ptr<WaitForCommandState> wait_for_get_offset_;
  uint32_t wait_set_get_buffer_count_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferStub);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_COMMAND_BUFFER_STUB_H_
