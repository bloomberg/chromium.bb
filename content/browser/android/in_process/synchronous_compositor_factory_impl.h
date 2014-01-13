// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_

#include "base/synchronization/lock.h"
#include "content/browser/android/in_process/synchronous_input_event_filter.h"
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "content/renderer/media/android/stream_texture_factory_android_synchronous_impl.h"

namespace gpu {
class GLInProcessContext;
}

namespace webkit {
namespace gpu {
class WebGraphicsContext3DInProcessCommandBufferImpl;
}
}

namespace content {

class SynchronousCompositorFactoryImpl : public SynchronousCompositorFactory {
 public:
  SynchronousCompositorFactoryImpl();
  virtual ~SynchronousCompositorFactoryImpl();

  // SynchronousCompositorFactory
  virtual scoped_refptr<base::MessageLoopProxy> GetCompositorMessageLoop()
      OVERRIDE;
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(int routing_id)
      OVERRIDE;
  virtual InputHandlerManagerClient* GetInputHandlerManagerClient() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      GetOffscreenContextProviderForMainThread() OVERRIDE;
  // This is called on both renderer main thread (offscreen context creation
  // path shared between cross-process and in-process platforms) and renderer
  // compositor impl thread (InitializeHwDraw) in order to support Android
  // WebView synchronously enable and disable hardware mode multiple times in
  // the same task. This is ok because in-process WGC3D creation may happen on
  // any thread and is lightweight.
  virtual scoped_refptr<cc::ContextProvider>
      GetOffscreenContextProviderForCompositorThread() OVERRIDE;
  virtual scoped_ptr<StreamTextureFactory> CreateStreamTextureFactory(
      int view_id) OVERRIDE;

  SynchronousInputEventFilter* synchronous_input_event_filter() {
    return &synchronous_input_event_filter_;
  }

  void CompositorInitializedHardwareDraw();
  void CompositorReleasedHardwareDraw();

 private:
  void ReleaseGlobalHardwareResources();
  bool CanCreateMainThreadContext();
  scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
      TryCreateStreamTextureFactory();
  scoped_ptr<webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl>
      CreateOffscreenContext();

  SynchronousInputEventFilter synchronous_input_event_filter_;

  // Only guards construction and destruction of
  // |offscreen_context_for_compositor_thread_|, not usage.
  base::Lock offscreen_context_for_compositor_thread_lock_;
  scoped_refptr<cc::ContextProvider> offscreen_context_for_main_thread_;
  // This is a pointer to the context owned by
  // |offscreen_context_for_main_thread_|.
  gpu::GLInProcessContext* wrapped_gl_context_for_main_thread_;
  scoped_refptr<cc::ContextProvider> offscreen_context_for_compositor_thread_;

  // |num_hardware_compositor_lock_| is updated on UI thread only but can be
  // read on renderer main thread.
  base::Lock num_hardware_compositor_lock_;
  unsigned int num_hardware_compositors_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_
