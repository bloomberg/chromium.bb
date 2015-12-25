// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/output_surface.h"
#include "content/renderer/gpu/compositor_forwarding_message_filter.h"
#include "ipc/ipc_sync_message_filter.h"

namespace IPC {
class Message;
}

namespace cc {
class CompositorFrame;
class CompositorFrameAck;
class GLFrameData;
}

namespace content {
class ContextProviderCommandBuffer;
class FrameSwapMessageQueue;

// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when bindToClient is called.
class CompositorOutputSurface
    : NON_EXPORTED_BASE(public cc::OutputSurface),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  CompositorOutputSurface(
      int32_t routing_id,
      uint32_t output_surface_id,
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
      const scoped_refptr<ContextProviderCommandBuffer>&
          worker_context_provider,
      scoped_ptr<cc::SoftwareOutputDevice> software,
      scoped_refptr<FrameSwapMessageQueue> swap_frame_message_queue,
      bool use_swap_compositor_frame_message);
  ~CompositorOutputSurface() override;

  // cc::OutputSurface implementation.
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  void DetachFromClient() override;
  void SwapBuffers(cc::CompositorFrame* frame) override;

  // TODO(epenner): This seems out of place here and would be a better fit
  // int CompositorThread after it is fully refactored (http://crbug/170828)
  void UpdateSmoothnessTakesPriority(bool prefer_smoothness) override;

 protected:
  void ShortcutSwapAck(uint32_t output_surface_id,
                       scoped_ptr<cc::GLFrameData> gl_frame_data);
  virtual void OnSwapAck(uint32_t output_surface_id,
                         const cc::CompositorFrameAck& ack);
  virtual void OnReclaimResources(uint32_t output_surface_id,
                                  const cc::CompositorFrameAck& ack);
  uint32_t output_surface_id_;

 private:
  class CompositorOutputSurfaceProxy :
      public base::RefCountedThreadSafe<CompositorOutputSurfaceProxy> {
   public:
    explicit CompositorOutputSurfaceProxy(
        CompositorOutputSurface* output_surface)
        : output_surface_(output_surface) {}
    void ClearOutputSurface() { output_surface_ = NULL; }
    void OnMessageReceived(const IPC::Message& message) {
      if (output_surface_)
        output_surface_->OnMessageReceived(message);
    }

   private:
    friend class base::RefCountedThreadSafe<CompositorOutputSurfaceProxy>;
    ~CompositorOutputSurfaceProxy() {}
    CompositorOutputSurface* output_surface_;

    DISALLOW_COPY_AND_ASSIGN(CompositorOutputSurfaceProxy);
  };

  void OnMessageReceived(const IPC::Message& message);
  void OnUpdateVSyncParametersFromBrowser(base::TimeTicks timebase,
                                          base::TimeDelta interval);
  bool Send(IPC::Message* message);

  bool use_swap_compositor_frame_message_;

  scoped_refptr<CompositorForwardingMessageFilter> output_surface_filter_;
  CompositorForwardingMessageFilter::Handler output_surface_filter_handler_;
  scoped_refptr<CompositorOutputSurfaceProxy> output_surface_proxy_;
  scoped_refptr<IPC::SyncMessageFilter> message_sender_;
  scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue_;
  int routing_id_;
#if defined(OS_ANDROID)
  bool prefers_smoothness_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;
#endif

  // TODO(danakj): Remove this when crbug.com/311404
  bool layout_test_mode_;
  scoped_ptr<cc::CompositorFrameAck> layout_test_previous_frame_ack_;
  base::WeakPtrFactory<CompositorOutputSurface> weak_ptrs_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_
