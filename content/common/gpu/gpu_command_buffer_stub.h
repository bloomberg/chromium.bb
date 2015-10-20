// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
#define CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_

#include <deque>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_memory_manager_client.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/base/video_decoder_config.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"
#include "url/gurl.h"

namespace gpu {
struct Mailbox;
class SyncPointClient;
class SyncPointManager;
class ValueStateMap;
namespace gles2 {
class MailboxManager;
class SubscriptionRefSet;
}
}

namespace content {

class GpuChannel;
class GpuVideoDecodeAccelerator;
class GpuVideoEncodeAccelerator;
class GpuWatchdog;
struct WaitForCommandState;

class GpuCommandBufferStub
    : public GpuMemoryManagerClient,
      public IPC::Listener,
      public IPC::Sender,
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

  GpuCommandBufferStub(
      GpuChannel* channel,
      gpu::SyncPointManager* sync_point_manager,
      base::SingleThreadTaskRunner* task_runner,
      GpuCommandBufferStub* share_group,
      const gfx::GLSurfaceHandle& handle,
      gpu::gles2::MailboxManager* mailbox_manager,
      gpu::PreemptionFlag* preempt_by_flag,
      gpu::gles2::SubscriptionRefSet* subscription_ref_set,
      gpu::ValueStateMap* pending_valuebuffer_state,
      const gfx::Size& size,
      const gpu::gles2::DisallowedFeatures& disallowed_features,
      const std::vector<int32>& attribs,
      gfx::GpuPreference gpu_preference,
      int32 stream_id,
      int32 route_id,
      bool offscreen,
      GpuWatchdog* watchdog,
      const GURL& active_url);

  ~GpuCommandBufferStub() override;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // GpuMemoryManagerClient implementation:
  gfx::Size GetSurfaceSize() const override;
  gpu::gles2::MemoryTracker* GetMemoryTracker() const override;
  void SuggestHaveFrontBuffer(bool suggest_have_frontbuffer) override;
  bool GetTotalGpuMemory(uint64* bytes) override;

  // Whether this command buffer can currently handle IPC messages.
  bool IsScheduled();

  // Whether there are commands in the buffer that haven't been processed.
  bool HasUnprocessedCommands();

  gpu::gles2::GLES2Decoder* decoder() const { return decoder_.get(); }
  gpu::GpuScheduler* scheduler() const { return scheduler_.get(); }
  GpuChannel* channel() const { return channel_; }

  // Unique command buffer ID for this command buffer stub.
  uint64_t command_buffer_id() const { return command_buffer_id_; }

  // Identifies the various GpuCommandBufferStubs in the GPU process belonging
  // to the same renderer process.
  int32 route_id() const { return route_id_; }

  // Identifies the stream for this command buffer.
  int32 stream_id() const { return stream_id_; }

  gfx::GpuPreference gpu_preference() { return gpu_preference_; }

  int32 GetRequestedAttribute(int attr) const;

  // Sends a message to the console.
  void SendConsoleMessage(int32 id, const std::string& message);

  void SendCachedShader(const std::string& key, const std::string& shader);

  gfx::GLSurface* surface() const { return surface_.get(); }

  void AddDestructionObserver(DestructionObserver* observer);
  void RemoveDestructionObserver(DestructionObserver* observer);

  // Associates a sync point to this stub. When the stub is destroyed, it will
  // retire all sync points that haven't been previously retired.
  void InsertSyncPoint(uint32 sync_point, bool retire);

  void SetLatencyInfoCallback(const LatencyInfoCallback& callback);

  void MarkContextLost();

  const gpu::gles2::FeatureInfo* GetFeatureInfo() const;

  void SendSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result);
  void SendUpdateVSyncParameters(base::TimeTicks timebase,
                                 base::TimeDelta interval);

 private:
  GpuMemoryManager* GetMemoryManager() const;

  void Destroy();

  bool MakeCurrent();

  // Cleans up and sends reply if OnInitialize failed.
  void OnInitializeFailed(IPC::Message* reply_message);

  // Message handlers:
  void OnInitialize(base::SharedMemoryHandle shared_state_shm,
                    IPC::Message* reply_message);
  void OnSetGetBuffer(int32 shm_id, IPC::Message* reply_message);
  void OnProduceFrontBuffer(const gpu::Mailbox& mailbox);
  void OnGetState(IPC::Message* reply_message);
  void OnWaitForTokenInRange(int32 start,
                             int32 end,
                             IPC::Message* reply_message);
  void OnWaitForGetOffsetInRange(int32 start,
                                 int32 end,
                                 IPC::Message* reply_message);
  void OnAsyncFlush(int32 put_offset, uint32 flush_count,
                    const std::vector<ui::LatencyInfo>& latency_info);
  void OnRegisterTransferBuffer(int32 id,
                                base::SharedMemoryHandle transfer_buffer,
                                uint32 size);
  void OnDestroyTransferBuffer(int32 id);
  void OnGetTransferBuffer(int32 id, IPC::Message* reply_message);

  void OnCreateVideoDecoder(media::VideoCodecProfile profile,
                            int32 route_id,
                            IPC::Message* reply_message);
  void OnCreateVideoEncoder(media::VideoPixelFormat input_format,
                            const gfx::Size& input_visible_size,
                            media::VideoCodecProfile output_profile,
                            uint32 initial_bitrate,
                            int32 route_id,
                            IPC::Message* reply_message);

  void OnSetSurfaceVisible(bool visible);

  void OnEnsureBackbuffer();

  void OnRetireSyncPoint(uint32 sync_point);
  bool OnWaitSyncPoint(uint32 sync_point);
  void OnWaitSyncPointCompleted(uint32 sync_point);
  void OnSignalSyncPoint(uint32 sync_point, uint32 id);
  void OnSignalSyncPointAck(uint32 id);
  void OnSignalQuery(uint32 query, uint32 id);

  void OnFenceSyncRelease(uint64_t release);
  bool OnWaitFenceSync(gpu::CommandBufferNamespace namespace_id,
                       uint64_t command_buffer_id,
                       uint64_t release);
  void OnWaitFenceSyncCompleted(gpu::CommandBufferNamespace namespace_id,
                                uint64_t command_buffer_id,
                                uint64_t release);

  void OnCreateImage(int32 id,
                     gfx::GpuMemoryBufferHandle handle,
                     gfx::Size size,
                     gfx::BufferFormat format,
                     uint32 internalformat);
  void OnDestroyImage(int32 id);
  void OnCreateStreamTexture(uint32 texture_id,
                             int32 stream_id,
                             bool* succeeded);

  void OnCommandProcessed();
  void OnParseError();
  void OnSchedulingChanged(bool scheduled);

  void ReportState();

  // Wrapper for GpuScheduler::PutChanged that sets the crash report URL.
  void PutChanged();

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
  void PullTextureUpdates(gpu::CommandBufferNamespace namespace_id,
                          uint64_t command_buffer_id,
                          uint32_t release);

  // The lifetime of objects of this class is managed by a GpuChannel. The
  // GpuChannels destroy all the GpuCommandBufferStubs that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannel* channel_;

  // Outlives the stub.
  gpu::SyncPointManager* sync_point_manager_;

  // Task runner for main thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The group of contexts that share namespaces with this context.
  scoped_refptr<gpu::gles2::ContextGroup> context_group_;

  bool initialized_;
  gfx::GLSurfaceHandle handle_;
  gfx::Size initial_size_;
  gpu::gles2::DisallowedFeatures disallowed_features_;
  std::vector<int32> requested_attribs_;
  gfx::GpuPreference gpu_preference_;
  bool use_virtualized_gl_context_;
  const uint64_t command_buffer_id_;
  const int32 stream_id_;
  const int32 route_id_;
  const bool offscreen_;
  uint32 last_flush_count_;

  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_ptr<gpu::GpuScheduler> scheduler_;
  scoped_ptr<gpu::SyncPointClient> sync_point_client_;
  scoped_refptr<gfx::GLSurface> surface_;

  GpuWatchdog* watchdog_;

  base::ObserverList<DestructionObserver> destruction_observers_;

  // A queue of sync points associated with this stub.
  std::deque<uint32> sync_points_;
  bool waiting_for_sync_point_;

  base::TimeTicks process_delayed_work_time_;
  uint32_t previous_processed_num_;
  base::TimeTicks last_idle_time_;

  scoped_refptr<gpu::PreemptionFlag> preemption_flag_;

  LatencyInfoCallback latency_info_callback_;

  GURL active_url_;
  size_t active_url_hash_;

  size_t total_gpu_memory_;
  scoped_ptr<WaitForCommandState> wait_for_token_;
  scoped_ptr<WaitForCommandState> wait_for_get_offset_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferStub);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
