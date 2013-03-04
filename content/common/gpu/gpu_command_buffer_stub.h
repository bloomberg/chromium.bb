// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
#define CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_

#include <deque>
#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_memory_manager_client.h"
#include "googleurl/src/gurl.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"
#include "ui/surface/transport_dib.h"

namespace gpu {
namespace gles2 {
class ImageManager;
class MailboxManager;
}
}

namespace content {

class GpuChannel;
class GpuVideoDecodeAccelerator;
class GpuWatchdog;

class GpuCommandBufferStub
    : public GpuMemoryManagerClient,
      public IPC::Listener,
      public IPC::Sender,
      public base::SupportsWeakPtr<GpuCommandBufferStub> {
 public:
  class DestructionObserver {
   public:
    // Called in Destroy(), before the context/surface are released.
    virtual void OnWillDestroyStub(GpuCommandBufferStub* stub) = 0;

   protected:
    virtual ~DestructionObserver() {}
  };

  GpuCommandBufferStub(
      GpuChannel* channel,
      GpuCommandBufferStub* share_group,
      const gfx::GLSurfaceHandle& handle,
      gpu::gles2::MailboxManager* mailbox_manager,
      gpu::gles2::ImageManager* image_manager,
      const gfx::Size& size,
      const gpu::gles2::DisallowedFeatures& disallowed_features,
      const std::string& allowed_extensions,
      const std::vector<int32>& attribs,
      gfx::GpuPreference gpu_preference,
      int32 route_id,
      int32 surface_id,
      GpuWatchdog* watchdog,
      bool software,
      const GURL& active_url);

  virtual ~GpuCommandBufferStub();

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // GpuMemoryManagerClient implementation:
  virtual gfx::Size GetSurfaceSize() const OVERRIDE;
  virtual gpu::gles2::MemoryTracker* GetMemoryTracker() const OVERRIDE;
  virtual void SetMemoryAllocation(
      const GpuMemoryAllocation& allocation) OVERRIDE;
  virtual bool GetTotalGpuMemory(uint64* bytes) OVERRIDE;

  // Whether this command buffer can currently handle IPC messages.
  bool IsScheduled();

  // If the command buffer is pre-empted and cannot process commands.
  bool IsPreempted() const {
    return scheduler_.get() && scheduler_->IsPreempted();
  }

  // Whether there are commands in the buffer that haven't been processed.
  bool HasUnprocessedCommands();

  gpu::gles2::GLES2Decoder* decoder() const { return decoder_.get(); }
  gpu::GpuScheduler* scheduler() const { return scheduler_.get(); }
  GpuChannel* channel() const { return channel_; }

  // Identifies the target surface.
  int32 surface_id() const {
    return surface_id_;
  }

  // Identifies the various GpuCommandBufferStubs in the GPU process belonging
  // to the same renderer process.
  int32 route_id() const { return route_id_; }

  gfx::GpuPreference gpu_preference() { return gpu_preference_; }

  // Sends a message to the console.
  void SendConsoleMessage(int32 id, const std::string& message);

  gfx::GLSurface* surface() const { return surface_; }

  void AddDestructionObserver(DestructionObserver* observer);
  void RemoveDestructionObserver(DestructionObserver* observer);

  // Associates a sync point to this stub. When the stub is destroyed, it will
  // retire all sync points that haven't been previously retired.
  void AddSyncPoint(uint32 sync_point);

  void SetPreemptByFlag(scoped_refptr<gpu::PreemptionFlag> flag);

 private:
  GpuMemoryManager* GetMemoryManager();
  bool MakeCurrent();
  void Destroy();

  // Cleans up and sends reply if OnInitialize failed.
  void OnInitializeFailed(IPC::Message* reply_message);

  // Message handlers:
  void OnInitialize(base::SharedMemoryHandle shared_state_shm,
                    IPC::Message* reply_message);
  void OnSetGetBuffer(int32 shm_id, IPC::Message* reply_message);
  void OnSetParent(int32 parent_route_id,
                   uint32 parent_texture_id,
                   IPC::Message* reply_message);
  void OnGetState(IPC::Message* reply_message);
  void OnGetStateFast(IPC::Message* reply_message);
  void OnAsyncFlush(int32 put_offset, uint32 flush_count);
  void OnEcho(const IPC::Message& message);
  void OnRescheduled();
  void OnRegisterTransferBuffer(int32 id,
                                base::SharedMemoryHandle transfer_buffer,
                                uint32 size);
  void OnDestroyTransferBuffer(int32 id);
  void OnGetTransferBuffer(int32 id, IPC::Message* reply_message);

  void OnCreateVideoDecoder(
      media::VideoCodecProfile profile,
      IPC::Message* reply_message);
  void OnDestroyVideoDecoder(int32 decoder_route_id);

  void OnSetSurfaceVisible(bool visible);

  void OnDiscardBackbuffer();
  void OnEnsureBackbuffer();

  void OnRetireSyncPoint(uint32 sync_point);
  bool OnWaitSyncPoint(uint32 sync_point);
  void OnSyncPointRetired();
  void OnSignalSyncPoint(uint32 sync_point, uint32 id);
  void OnSignalSyncPointAck(uint32 id);

  void OnReceivedClientManagedMemoryStats(const GpuManagedMemoryStats& stats);
  void OnSetClientHasMemoryAllocationChangedCallback(bool has_callback);

  void OnCommandProcessed();
  void OnParseError();

  void ReportState();

  // Wrapper for GpuScheduler::PutChanged that sets the crash report URL.
  void PutChanged();

  // Poll the command buffer to execute work.
  void PollWork();

  // Whether this command buffer needs to be polled again in the future.
  bool HasMoreWork();

  void ScheduleDelayedWork(int64 delay);

  // The lifetime of objects of this class is managed by a GpuChannel. The
  // GpuChannels destroy all the GpuCommandBufferStubs that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannel* channel_;

  // The group of contexts that share namespaces with this context.
  scoped_refptr<gpu::gles2::ContextGroup> context_group_;

  gfx::GLSurfaceHandle handle_;
  gfx::Size initial_size_;
  gpu::gles2::DisallowedFeatures disallowed_features_;
  std::string allowed_extensions_;
  std::vector<int32> requested_attribs_;
  gfx::GpuPreference gpu_preference_;
  int32 route_id_;
  int32 surface_id_;
  bool software_;
  uint32 last_flush_count_;

  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_ptr<gpu::GpuScheduler> scheduler_;
  scoped_refptr<gfx::GLSurface> surface_;

  scoped_ptr<GpuMemoryManagerClientState> memory_manager_client_state_;
  // The last memory allocation received from the GpuMemoryManager (used to
  // elide redundant work).
  bool last_memory_allocation_valid_;
  GpuMemoryAllocation last_memory_allocation_;

  // SetParent may be called before Initialize, in which case we need to keep
  // around the parent stub, so that Initialize can set the parent correctly.
  base::WeakPtr<GpuCommandBufferStub> parent_stub_for_initialization_;
  uint32 parent_texture_for_initialization_;

  GpuWatchdog* watchdog_;

  // Zero or more video decoders owned by this stub, keyed by their
  // decoder_route_id.
  IDMap<GpuVideoDecodeAccelerator, IDMapOwnPointer> video_decoders_;

  ObserverList<DestructionObserver> destruction_observers_;

  // A queue of sync points associated with this stub.
  std::deque<uint32> sync_points_;
  int sync_point_wait_count_;

  bool delayed_work_scheduled_;

  scoped_refptr<gpu::PreemptionFlag> preemption_flag_;

  GURL active_url_;
  size_t active_url_hash_;

  size_t total_gpu_memory_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferStub);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
