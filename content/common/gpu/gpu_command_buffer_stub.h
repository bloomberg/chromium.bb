// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
#define CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/video/video_decode_accelerator.h"
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
struct SyncToken;
class SyncPointClient;
class SyncPointManager;
class ValueStateMap;
namespace gles2 {
class MailboxManager;
class SubscriptionRefSet;
}
}

struct GpuCommandBufferMsg_CreateImage_Params;

namespace content {

class GpuChannel;
class GpuVideoDecodeAccelerator;
class GpuVideoEncodeAccelerator;
class GpuWatchdog;
struct WaitForCommandState;

class GpuCommandBufferStub
    : public IPC::Listener,
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
      const std::vector<int32_t>& attribs,
      gfx::GpuPreference gpu_preference,
      int32_t stream_id,
      int32_t route_id,
      bool offscreen,
      GpuWatchdog* watchdog,
      const GURL& active_url);

  ~GpuCommandBufferStub() override;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  gpu::gles2::MemoryTracker* GetMemoryTracker() const;

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
  int32_t route_id() const { return route_id_; }

  // Identifies the stream for this command buffer.
  int32_t stream_id() const { return stream_id_; }

  gfx::GpuPreference gpu_preference() { return gpu_preference_; }

  int32_t GetRequestedAttribute(int attr) const;

  // Sends a message to the console.
  void SendConsoleMessage(int32_t id, const std::string& message);

  void SendCachedShader(const std::string& key, const std::string& shader);

  gfx::GLSurface* surface() const { return surface_.get(); }

  void AddDestructionObserver(DestructionObserver* observer);
  void RemoveDestructionObserver(DestructionObserver* observer);

  // Associates a sync point to this stub. When the stub is destroyed, it will
  // retire all sync points that haven't been previously retired.
  void InsertSyncPoint(uint32_t sync_point, bool retire);

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
  void OnSetGetBuffer(int32_t shm_id, IPC::Message* reply_message);
  void OnProduceFrontBuffer(const gpu::Mailbox& mailbox);
  void OnGetState(IPC::Message* reply_message);
  void OnWaitForTokenInRange(int32_t start,
                             int32_t end,
                             IPC::Message* reply_message);
  void OnWaitForGetOffsetInRange(int32_t start,
                                 int32_t end,
                                 IPC::Message* reply_message);
  void OnAsyncFlush(int32_t put_offset,
                    uint32_t flush_count,
                    const std::vector<ui::LatencyInfo>& latency_info);
  void OnRegisterTransferBuffer(int32_t id,
                                base::SharedMemoryHandle transfer_buffer,
                                uint32_t size);
  void OnDestroyTransferBuffer(int32_t id);
  void OnGetTransferBuffer(int32_t id, IPC::Message* reply_message);

  void OnCreateVideoDecoder(const media::VideoDecodeAccelerator::Config& config,
                            int32_t route_id,
                            IPC::Message* reply_message);
  void OnCreateVideoEncoder(media::VideoPixelFormat input_format,
                            const gfx::Size& input_visible_size,
                            media::VideoCodecProfile output_profile,
                            uint32_t initial_bitrate,
                            int32_t route_id,
                            IPC::Message* reply_message);

  void OnEnsureBackbuffer();

  void OnRetireSyncPoint(uint32_t sync_point);
  bool OnWaitSyncPoint(uint32_t sync_point);
  void OnWaitSyncPointCompleted(uint32_t sync_point);
  void OnSignalSyncPoint(uint32_t sync_point, uint32_t id);
  void OnSignalSyncToken(const gpu::SyncToken& sync_token, uint32_t id);
  void OnSignalAck(uint32_t id);
  void OnSignalQuery(uint32_t query, uint32_t id);

  void OnFenceSyncRelease(uint64_t release);
  bool OnWaitFenceSync(gpu::CommandBufferNamespace namespace_id,
                       uint64_t command_buffer_id,
                       uint64_t release);
  void OnWaitFenceSyncCompleted(gpu::CommandBufferNamespace namespace_id,
                                uint64_t command_buffer_id,
                                uint64_t release);

  void OnCreateImage(const GpuCommandBufferMsg_CreateImage_Params& params);
  void OnDestroyImage(int32_t id);
  void OnCreateStreamTexture(uint32_t texture_id,
                             int32_t stream_id,
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
  std::vector<int32_t> requested_attribs_;
  gfx::GpuPreference gpu_preference_;
  bool use_virtualized_gl_context_;
  const uint64_t command_buffer_id_;
  const int32_t stream_id_;
  const int32_t route_id_;
  const bool offscreen_;
  uint32_t last_flush_count_;

  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_ptr<gpu::GpuScheduler> scheduler_;
  scoped_ptr<gpu::SyncPointClient> sync_point_client_;
  scoped_refptr<gfx::GLSurface> surface_;

  GpuWatchdog* watchdog_;

  base::ObserverList<DestructionObserver> destruction_observers_;

  // A queue of sync points associated with this stub.
  std::deque<uint32_t> sync_points_;
  bool waiting_for_sync_point_;

  base::TimeTicks process_delayed_work_time_;
  uint32_t previous_processed_num_;
  base::TimeTicks last_idle_time_;

  scoped_refptr<gpu::PreemptionFlag> preemption_flag_;

  LatencyInfoCallback latency_info_callback_;

  GURL active_url_;
  size_t active_url_hash_;

  scoped_ptr<WaitForCommandState> wait_for_token_;
  scoped_ptr<WaitForCommandState> wait_for_get_offset_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferStub);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
