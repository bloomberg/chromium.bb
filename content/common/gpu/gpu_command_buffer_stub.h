// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
#define CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
#pragma once

#if defined(ENABLE_GPU)

#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/media/gpu_video_decode_accelerator.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/gl/gpu_preference.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_MACOSX)
#include "ui/surface/accelerated_surface_mac.h"
#endif

class GpuChannel;
struct GpuMemoryAllocation;
class GpuWatchdog;

namespace gpu {
namespace gles2 {
class MailboxManager;
}
}

// This Base class is used to expose methods of GpuCommandBufferStub used for
// testability.
class CONTENT_EXPORT GpuCommandBufferStubBase {
 public:
  struct CONTENT_EXPORT SurfaceState {
    int32 surface_id;
    bool visible;
    base::TimeTicks last_used_time;

    SurfaceState(int32 surface_id,
                 bool visible,
                 base::TimeTicks last_used_time);
  };

 public:
  virtual ~GpuCommandBufferStubBase() {}

  // Will not have surface state if this is an offscreen commandbuffer.
  virtual bool client_has_memory_allocation_changed_callback() const = 0;
  virtual bool has_surface_state() const = 0;
  virtual const SurfaceState& surface_state() const = 0;

  virtual gfx::Size GetSurfaceSize() const = 0;

  virtual bool IsInSameContextShareGroup(
      const GpuCommandBufferStubBase& other) const = 0;

  virtual void SendMemoryAllocationToProxy(
      const GpuMemoryAllocation& allocation) = 0;

  virtual void SetMemoryAllocation(
      const GpuMemoryAllocation& allocation) = 0;
};

class GpuCommandBufferStub
    : public GpuCommandBufferStubBase,
      public IPC::Channel::Listener,
      public IPC::Message::Sender,
      public base::SupportsWeakPtr<GpuCommandBufferStub> {
 public:
  class DestructionObserver {
   public:
    ~DestructionObserver() {}

    // Called in Destroy(), before the context/surface are released.
    virtual void OnWillDestroyStub(GpuCommandBufferStub* stub) = 0;
  };

  GpuCommandBufferStub(
      GpuChannel* channel,
      GpuCommandBufferStub* share_group,
      const gfx::GLSurfaceHandle& handle,
      gpu::gles2::MailboxManager* mailbox_manager,
      const gfx::Size& size,
      const gpu::gles2::DisallowedFeatures& disallowed_features,
      const std::string& allowed_extensions,
      const std::vector<int32>& attribs,
      gfx::GpuPreference gpu_preference,
      int32 route_id,
      int32 surface_id,
      GpuWatchdog* watchdog,
      bool software);

  virtual ~GpuCommandBufferStub();

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // GpuCommandBufferStubBase implementation:
  virtual bool client_has_memory_allocation_changed_callback() const OVERRIDE;
  virtual bool has_surface_state() const OVERRIDE;
  virtual const GpuCommandBufferStubBase::SurfaceState& surface_state() const
      OVERRIDE;

  // Returns surface size.
  virtual gfx::Size GetSurfaceSize() const OVERRIDE;

  // Returns true iff |other| is in the same context share group as this stub.
  virtual bool IsInSameContextShareGroup(
      const GpuCommandBufferStubBase& other) const OVERRIDE;

  // Sends memory allocation limits to render process.
  virtual void SendMemoryAllocationToProxy(
      const GpuMemoryAllocation& allocation) OVERRIDE;

  // Sets buffer usage depending on Memory Allocation
  virtual void SetMemoryAllocation(
      const GpuMemoryAllocation& allocation) OVERRIDE;

  // Whether this command buffer can currently handle IPC messages.
  bool IsScheduled();

  // Whether this command buffer needs to be polled again in the future.
  bool HasMoreWork();

  // Poll the command buffer to execute work.
  void PollWork();

  // Whether there are commands in the buffer that haven't been processed.
  bool HasUnprocessedCommands();

  // Delay an echo message until the command buffer has been rescheduled.
  void DelayEcho(IPC::Message*);

  gpu::gles2::GLES2Decoder* decoder() const { return decoder_.get(); }
  gpu::GpuScheduler* scheduler() const { return scheduler_.get(); }

  // Identifies the target surface.
  int32 surface_id() const {
    return (surface_state_.get()) ? surface_state_->surface_id : 0;
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

 private:
  void Destroy();

  // Cleans up and sends reply if OnInitialize failed.
  void OnInitializeFailed(IPC::Message* reply_message);

  // Message handlers:
  void OnInitialize(IPC::Message* reply_message);
  void OnSetGetBuffer(int32 shm_id, IPC::Message* reply_message);
  void OnSetSharedStateBuffer(int32 shm_id, IPC::Message* reply_message);
  void OnSetParent(int32 parent_route_id,
                   uint32 parent_texture_id,
                   IPC::Message* reply_message);
  void OnGetState(IPC::Message* reply_message);
  void OnGetStateFast(IPC::Message* reply_message);
  void OnAsyncFlush(int32 put_offset, uint32 flush_count);
  void OnEcho(const IPC::Message& message);
  void OnRescheduled();
  void OnCreateTransferBuffer(int32 size,
                              int32 id_request,
                              IPC::Message* reply_message);
  void OnRegisterTransferBuffer(base::SharedMemoryHandle transfer_buffer,
                                size_t size,
                                int32 id_request,
                                IPC::Message* reply_message);
  void OnDestroyTransferBuffer(int32 id, IPC::Message* reply_message);
  void OnGetTransferBuffer(int32 id, IPC::Message* reply_message);

  void OnCreateVideoDecoder(
      media::VideoCodecProfile profile,
      IPC::Message* reply_message);
  void OnDestroyVideoDecoder(int32 decoder_route_id);

  void OnSetSurfaceVisible(bool visible);

  void OnDiscardBackbuffer();
  void OnEnsureBackbuffer();

  void OnSetClientHasMemoryAllocationChangedCallback(bool);

  void OnReschedule();

  void OnCommandProcessed();
  void OnParseError();

  void ReportState();

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
  bool software_;
  bool client_has_memory_allocation_changed_callback_;
  uint32 last_flush_count_;
  scoped_ptr<GpuCommandBufferStubBase::SurfaceState> surface_state_;
  GpuMemoryAllocation allocation_;

  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_ptr<gpu::GpuScheduler> scheduler_;
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;

  // SetParent may be called before Initialize, in which case we need to keep
  // around the parent stub, so that Initialize can set the parent correctly.
  base::WeakPtr<GpuCommandBufferStub> parent_stub_for_initialization_;
  uint32 parent_texture_for_initialization_;

  GpuWatchdog* watchdog_;

  std::deque<IPC::Message*> delayed_echos_;

  // Zero or more video decoders owned by this stub, keyed by their
  // decoder_route_id.
  IDMap<GpuVideoDecodeAccelerator, IDMapOwnPointer> video_decoders_;

  ObserverList<DestructionObserver> destruction_observers_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferStub);
};

#endif  // defined(ENABLE_GPU)

#endif  // CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
