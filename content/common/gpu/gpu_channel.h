// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_CHANNEL_H_
#define CONTENT_COMMON_GPU_GPU_CHANNEL_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_stream_priority.h"
#include "content/common/message_router.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "ipc/ipc_sync_channel.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gpu_preference.h"

struct GPUCreateCommandBufferConfig;

namespace base {
class WaitableEvent;
}

namespace gpu {
class PreemptionFlag;
class SyncPointOrderData;
class SyncPointManager;
union ValueState;
class ValueStateMap;
namespace gles2 {
class SubscriptionRefSet;
}
}

namespace IPC {
class MessageFilter;
}

namespace content {
class GpuChannelManager;
class GpuChannelMessageFilter;
class GpuChannelMessageQueue;
class GpuJpegDecodeAccelerator;
class GpuWatchdog;

// Encapsulates an IPC channel between the GPU process and one renderer
// process. On the renderer side there's a corresponding GpuChannelHost.
class CONTENT_EXPORT GpuChannel
    : public IPC::Listener,
      public IPC::Sender,
      public gpu::gles2::SubscriptionRefSet::Observer {
 public:
  // Takes ownership of the renderer process handle.
  GpuChannel(GpuChannelManager* gpu_channel_manager,
             gpu::SyncPointManager* sync_point_manager,
             GpuWatchdog* watchdog,
             gfx::GLShareGroup* share_group,
             gpu::gles2::MailboxManager* mailbox_manager,
             gpu::PreemptionFlag* preempting_flag,
             base::SingleThreadTaskRunner* task_runner,
             base::SingleThreadTaskRunner* io_task_runner,
             int client_id,
             uint64_t client_tracing_id,
             bool allow_view_command_buffers,
             bool allow_real_time_streams);
  ~GpuChannel() override;

  // Initializes the IPC channel. Caller takes ownership of the client FD in
  // the returned handle and is responsible for closing it.
  virtual IPC::ChannelHandle Init(base::WaitableEvent* shutdown_event);

  // Get the GpuChannelManager that owns this channel.
  GpuChannelManager* gpu_channel_manager() const {
    return gpu_channel_manager_;
  }

  const std::string& channel_id() const { return channel_id_; }

  virtual base::ProcessId GetClientPID() const;

  int client_id() const { return client_id_; }

  uint64_t client_tracing_id() const { return client_tracing_id_; }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() const {
    return io_task_runner_;
  }

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelError() override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // SubscriptionRefSet::Observer implementation
  void OnAddSubscription(unsigned int target) override;
  void OnRemoveSubscription(unsigned int target) override;

  // This is called when a command buffer transitions between scheduled and
  // descheduled states. When any stub is descheduled, we stop preempting
  // other channels.
  void OnStubSchedulingChanged(GpuCommandBufferStub* stub, bool scheduled);

  gfx::GLShareGroup* share_group() const { return share_group_.get(); }

  GpuCommandBufferStub* LookupCommandBuffer(int32_t route_id);

  void LoseAllContexts();
  void MarkAllContextsLost();

  // Called to add a listener for a particular message routing ID.
  // Returns true if succeeded.
  bool AddRoute(int32_t route_id, IPC::Listener* listener);

  // Called to remove a listener for a particular message routing ID.
  void RemoveRoute(int32_t route_id);

  void SetPreemptingFlag(gpu::PreemptionFlag* flag);

  // If |preemption_flag->IsSet()|, any stub on this channel
  // should stop issuing GL commands. Setting this to NULL stops deferral.
  void SetPreemptByFlag(
      scoped_refptr<gpu::PreemptionFlag> preemption_flag);

  void CacheShader(const std::string& key, const std::string& shader);

  void AddFilter(IPC::MessageFilter* filter);
  void RemoveFilter(IPC::MessageFilter* filter);

  uint64_t GetMemoryUsage();

  scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      uint32_t internalformat);

  void HandleUpdateValueState(unsigned int target,
                              const gpu::ValueState& state);

  // Visible for testing.
  const gpu::ValueStateMap* pending_valuebuffer_state() const {
    return pending_valuebuffer_state_.get();
  }

  // Visible for testing.
  GpuChannelMessageFilter* filter() const { return filter_.get(); }

  // Returns the global order number for the last processed IPC message.
  uint32_t GetProcessedOrderNum() const;

  // Returns the global order number for the last unprocessed IPC message.
  uint32_t GetUnprocessedOrderNum() const;

  // Returns the shared sync point global order data.
  scoped_refptr<gpu::SyncPointOrderData> GetSyncPointOrderData();

  void HandleMessage();

  // Some messages such as WaitForGetOffsetInRange and WaitForTokenInRange are
  // processed as soon as possible because the client is blocked until they
  // are completed.
  void HandleOutOfOrderMessage(const IPC::Message& msg);

  // Synchronously handle the message to make testing convenient.
  void HandleMessageForTesting(const IPC::Message& msg);

#if defined(OS_ANDROID)
  const GpuCommandBufferStub* GetOneStub() const;
#endif

 protected:
  // The message filter on the io thread.
  scoped_refptr<GpuChannelMessageFilter> filter_;

  // Map of routing id to command buffer stub.
  base::ScopedPtrHashMap<int32_t, scoped_ptr<GpuCommandBufferStub>> stubs_;

 private:
  class StreamState {
   public:
    StreamState(int32_t id, GpuStreamPriority priority);
    ~StreamState();

    int32_t id() const { return id_; }
    GpuStreamPriority priority() const { return priority_; }

    void AddRoute(int32_t route_id);
    void RemoveRoute(int32_t route_id);
    bool HasRoute(int32_t route_id) const;
    bool HasRoutes() const;

   private:
    int32_t id_;
    GpuStreamPriority priority_;
    base::hash_set<int32_t> routes_;
  };

  void OnDestroy();

  bool OnControlMessageReceived(const IPC::Message& msg);

  void ScheduleHandleMessage();
  void HandleMessageHelper(const IPC::Message& msg);

  // Message handlers.
  void OnCreateViewCommandBuffer(
      const gfx::GLSurfaceHandle& window,
      const GPUCreateCommandBufferConfig& init_params,
      int32_t route_id,
      bool* succeeded);
  void OnCreateOffscreenCommandBuffer(
      const gfx::Size& size,
      const GPUCreateCommandBufferConfig& init_params,
      int32_t route_id,
      bool* succeeded);
  void OnDestroyCommandBuffer(int32_t route_id);
  void OnCreateJpegDecoder(int32_t route_id, IPC::Message* reply_msg);

  bool CreateCommandBuffer(const gfx::GLSurfaceHandle& window,
                           const gfx::Size& size,
                           const GPUCreateCommandBufferConfig& init_params,
                           int32_t route_id);

  // The lifetime of objects of this class is managed by a GpuChannelManager.
  // The GpuChannelManager destroy all the GpuChannels that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannelManager* gpu_channel_manager_;

  // Sync point manager. Outlives the channel and is guaranteed to outlive the
  // message loop.
  gpu::SyncPointManager* sync_point_manager_;

  scoped_ptr<IPC::SyncChannel> channel_;

  // Uniquely identifies the channel within this GPU process.
  std::string channel_id_;

  // Used to implement message routing functionality to CommandBuffer objects
  MessageRouter router_;

  // Whether the processing of IPCs on this channel is stalled and we should
  // preempt other GpuChannels.
  scoped_refptr<gpu::PreemptionFlag> preempting_flag_;

  // If non-NULL, all stubs on this channel should stop processing GL
  // commands (via their GpuScheduler) when preempted_flag_->IsSet()
  scoped_refptr<gpu::PreemptionFlag> preempted_flag_;

  scoped_refptr<GpuChannelMessageQueue> message_queue_;

  // The id of the client who is on the other side of the channel.
  int client_id_;

  // The tracing ID used for memory allocations associated with this client.
  uint64_t client_tracing_id_;

  // The task runners for the main thread and the io thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The share group that all contexts associated with a particular renderer
  // process use.
  scoped_refptr<gfx::GLShareGroup> share_group_;

  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;

  scoped_refptr<gpu::gles2::SubscriptionRefSet> subscription_ref_set_;

  scoped_refptr<gpu::ValueStateMap> pending_valuebuffer_state_;

  scoped_ptr<GpuJpegDecodeAccelerator> jpeg_decoder_;

  gpu::gles2::DisallowedFeatures disallowed_features_;
  GpuWatchdog* watchdog_;

  size_t num_stubs_descheduled_;

  // Map of stream id to stream state.
  base::hash_map<int32_t, StreamState> streams_;

  bool allow_view_command_buffers_;
  bool allow_real_time_streams_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannel);
};

// This filter does three things:
// - it counts and timestamps each message forwarded to the channel
//   so that we can preempt other channels if a message takes too long to
//   process. To guarantee fairness, we must wait a minimum amount of time
//   before preempting and we limit the amount of time that we can preempt in
//   one shot (see constants above).
// - it handles the GpuCommandBufferMsg_InsertSyncPoint message on the IO
//   thread, generating the sync point ID and responding immediately, and then
//   posting a task to insert the GpuCommandBufferMsg_RetireSyncPoint message
//   into the channel's queue.
// - it generates mailbox names for clients of the GPU process on the IO thread.
class GpuChannelMessageFilter : public IPC::MessageFilter {
 public:
  GpuChannelMessageFilter(const base::WeakPtr<GpuChannel>& gpu_channel,
                          GpuChannelMessageQueue* message_queue,
                          base::SingleThreadTaskRunner* task_runner,
                          gpu::PreemptionFlag* preempting_flag);

  // IPC::MessageFilter implementation.
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void AddChannelFilter(scoped_refptr<IPC::MessageFilter> filter);
  void RemoveChannelFilter(scoped_refptr<IPC::MessageFilter> filter);

  void OnMessageProcessed();

  void UpdateStubSchedulingState(bool a_stub_is_descheduled);

  bool Send(IPC::Message* message);

 protected:
  ~GpuChannelMessageFilter() override;

 private:
  enum PreemptionState {
    // Either there's no other channel to preempt, there are no messages
    // pending processing, or we just finished preempting and have to wait
    // before preempting again.
    IDLE,
    // We are waiting kPreemptWaitTimeMs before checking if we should preempt.
    WAITING,
    // We can preempt whenever any IPC processing takes more than
    // kPreemptWaitTimeMs.
    CHECKING,
    // We are currently preempting (i.e. no stub is descheduled).
    PREEMPTING,
    // We would like to preempt, but some stub is descheduled.
    WOULD_PREEMPT_DESCHEDULED,
  };

  void UpdatePreemptionState();

  void TransitionToIdleIfCaughtUp();
  void TransitionToIdle();
  void TransitionToWaiting();
  void TransitionToChecking();
  void TransitionToPreempting();
  void TransitionToWouldPreemptDescheduled();

  PreemptionState preemption_state_;

  // Maximum amount of time that we can spend in PREEMPTING.
  // It is reset when we transition to IDLE.
  base::TimeDelta max_preemption_time_;

  base::WeakPtr<GpuChannel> gpu_channel_;
  // The message_queue_ is used to handle messages on the main thread.
  scoped_refptr<GpuChannelMessageQueue> message_queue_;
  IPC::Sender* sender_;
  base::ProcessId peer_pid_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<gpu::PreemptionFlag> preempting_flag_;
  std::vector<scoped_refptr<IPC::MessageFilter>> channel_filters_;

  // This timer is created and destroyed on the IO thread.
  scoped_ptr<base::OneShotTimer> timer_;

  bool a_stub_is_descheduled_;
};

struct GpuChannelMessage {
  uint32_t order_number;
  base::TimeTicks time_received;
  IPC::Message message;

  GpuChannelMessage(const IPC::Message& msg)
      : order_number(0),
        time_received(base::TimeTicks()),
        message(msg) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuChannelMessage);
};

class GpuChannelMessageQueue
    : public base::RefCountedThreadSafe<GpuChannelMessageQueue> {
 public:
  static scoped_refptr<GpuChannelMessageQueue> Create(
      const base::WeakPtr<GpuChannel>& gpu_channel,
      base::SingleThreadTaskRunner* task_runner,
      gpu::SyncPointManager* sync_point_manager);

  scoped_refptr<gpu::SyncPointOrderData> GetSyncPointOrderData();

  // Returns the global order number for the last unprocessed IPC message.
  uint32_t GetUnprocessedOrderNum() const;

  // Returns the global order number for the last unprocessed IPC message.
  uint32_t GetProcessedOrderNum() const;

  bool HasQueuedMessages() const;

  base::TimeTicks GetNextMessageTimeTick() const;

  GpuChannelMessage* GetNextMessage() const;

  // Should be called before a message begins to be processed.
  void BeginMessageProcessing(const GpuChannelMessage* msg);

  // Should be called if a message began processing but did not finish.
  void PauseMessageProcessing(const GpuChannelMessage* msg);

  // Should be called after a message returned by GetNextMessage is processed.
  // Returns true if there are more messages on the queue.
  bool MessageProcessed();

  void PushBackMessage(const IPC::Message& message);

  void DeleteAndDisableMessages();

 private:
  friend class base::RefCountedThreadSafe<GpuChannelMessageQueue>;

  GpuChannelMessageQueue(const base::WeakPtr<GpuChannel>& gpu_channel,
                         base::SingleThreadTaskRunner* task_runner,
                         gpu::SyncPointManager* sync_point_manager);
  ~GpuChannelMessageQueue();

  void ScheduleHandleMessage();

  void PushMessageHelper(scoped_ptr<GpuChannelMessage> msg);

  bool enabled_;

  // Both deques own the messages.
  std::deque<GpuChannelMessage*> channel_messages_;

  // This lock protects enabled_ and channel_messages_.
  mutable base::Lock channel_messages_lock_;

  // Keeps track of sync point related state such as message order numbers.
  scoped_refptr<gpu::SyncPointOrderData> sync_point_order_data_;

  base::WeakPtr<GpuChannel> gpu_channel_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  gpu::SyncPointManager* sync_point_manager_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelMessageQueue);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_CHANNEL_H_
