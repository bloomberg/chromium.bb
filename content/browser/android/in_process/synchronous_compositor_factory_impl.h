// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_

#include "base/synchronization/lock.h"
#include "cc/blink/context_provider_web_context.h"
#include "content/browser/android/in_process/synchronous_input_event_filter.h"
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "content/renderer/media/android/stream_texture_factory_synchronous_impl.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"

namespace gpu {
class GLInProcessContext;
}

namespace gpu_blink {
class WebGraphicsContext3DInProcessCommandBufferImpl;
}

namespace content {

class SynchronousCompositorFactoryImpl : public SynchronousCompositorFactory {
 public:
  SynchronousCompositorFactoryImpl();
  ~SynchronousCompositorFactoryImpl() override;

  // SynchronousCompositorFactory
  scoped_refptr<base::MessageLoopProxy> GetCompositorMessageLoop() override;
  bool RecordFullLayer() override;
  scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      int routing_id,
      scoped_refptr<content::FrameSwapMessageQueue> frame_swap_message_queue)
      override;
  InputHandlerManagerClient* GetInputHandlerManagerClient() override;
  scoped_ptr<cc::BeginFrameSource> CreateExternalBeginFrameSource(
      int routing_id) override;
  scoped_refptr<cc_blink::ContextProviderWebContext>
  CreateOffscreenContextProvider(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      const std::string& debug_name) override;
  scoped_refptr<StreamTextureFactory> CreateStreamTextureFactory(
      int view_id) override;
  gpu_blink::WebGraphicsContext3DInProcessCommandBufferImpl*
  CreateOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes) override;

  SynchronousInputEventFilter* synchronous_input_event_filter() {
    return &synchronous_input_event_filter_;
  }

  void SetDeferredGpuService(
      scoped_refptr<gpu::InProcessCommandBuffer::Service> service);
  void SetRecordFullDocument(bool record_full_document);
  void CompositorInitializedHardwareDraw();
  void CompositorReleasedHardwareDraw();

  scoped_refptr<cc::ContextProvider> CreateContextProviderForCompositor();

 private:
  bool CanCreateMainThreadContext();
  scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
      TryCreateStreamTextureFactory();
  void RestoreContextOnMainThread();

  SynchronousInputEventFilter synchronous_input_event_filter_;

  scoped_refptr<gpu::InProcessCommandBuffer::Service> service_;

  class VideoContextProvider;
  scoped_refptr<VideoContextProvider> video_context_provider_;

  bool record_full_layer_;

  // |num_hardware_compositor_lock_| is updated on UI thread only but can be
  // read on renderer main thread.
  base::Lock num_hardware_compositor_lock_;
  unsigned int num_hardware_compositors_;
  scoped_refptr<base::MessageLoopProxy> main_thread_proxy_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_
