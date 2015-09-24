// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_

#include "base/synchronization/lock.h"
#include "cc/blink/context_provider_web_context.h"
#include "content/browser/android/in_process/synchronous_input_event_filter.h"
#include "content/common/gpu/client/command_buffer_metrics.h"
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "content/renderer/media/android/stream_texture_factory_synchronous_impl.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"

namespace base {
class Thread;
}

namespace gpu {
class GLInProcessContext;
}

namespace gpu_blink {
class WebGraphicsContext3DInProcessCommandBufferImpl;
}

namespace content {

class InProcessChildThreadParams;
class SynchronousCompositorContextProvider;

class SynchronousCompositorFactoryImpl : public SynchronousCompositorFactory {
 public:
  SynchronousCompositorFactoryImpl();
  ~SynchronousCompositorFactoryImpl() override;

  // SynchronousCompositorFactory
  scoped_refptr<base::SingleThreadTaskRunner> GetCompositorTaskRunner()
      override;
  scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      int routing_id,
      int surface_id,
      scoped_refptr<content::FrameSwapMessageQueue> frame_swap_message_queue)
      override;
  InputHandlerManagerClient* GetInputHandlerManagerClient() override;
  scoped_ptr<cc::BeginFrameSource> CreateExternalBeginFrameSource(
      int routing_id) override;
  scoped_refptr<StreamTextureFactory> CreateStreamTextureFactory(
      int view_id) override;
  bool OverrideWithFactory() override;
  scoped_refptr<cc_blink::ContextProviderWebContext>
  CreateOffscreenContextProvider(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      const std::string& debug_name) override;
  gpu_blink::WebGraphicsContext3DInProcessCommandBufferImpl*
  CreateOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes) override;
  gpu::GPUInfo GetGPUInfo() const override;

  SynchronousInputEventFilter* synchronous_input_event_filter() {
    return &synchronous_input_event_filter_;
  }

  void SetDeferredGpuService(
      scoped_refptr<gpu::InProcessCommandBuffer::Service> service);
  base::Thread* CreateInProcessGpuThread(
      const InProcessChildThreadParams& params);
  void SetUseIpcCommandBuffer();
  void CompositorInitializedHardwareDraw();
  void CompositorReleasedHardwareDraw();


 private:
  scoped_refptr<cc::ContextProvider> CreateContextProviderForCompositor(
      int surface_id,
      CommandBufferContextType type);
  scoped_refptr<cc::ContextProvider> GetSharedWorkerContextProvider();
  bool CanCreateMainThreadContext();
  scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
      TryCreateStreamTextureFactory();
  void RestoreContextOnMainThread();
  scoped_refptr<gpu::InProcessCommandBuffer::Service> GpuThreadService();

  SynchronousInputEventFilter synchronous_input_event_filter_;

  scoped_refptr<gpu::InProcessCommandBuffer::Service> android_view_service_;
  scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_thread_service_;

  class VideoContextProvider;
  scoped_refptr<VideoContextProvider> video_context_provider_;

  scoped_refptr<SynchronousCompositorContextProvider> shared_worker_context_;
  scoped_refptr<cc_blink::ContextProviderWebContext>
      in_process_shared_worker_context_;

  bool use_ipc_command_buffer_;

  // |num_hardware_compositor_lock_| is updated on UI thread only but can be
  // read on renderer main thread.
  base::Lock num_hardware_compositor_lock_;
  unsigned int num_hardware_compositors_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_
