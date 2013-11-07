// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_impl.h"

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "cc/input/input_handler.h"
#include "cc/input/layer_scroll_offset_delegate.h"
#include "content/browser/android/in_process/synchronous_input_event_filter.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "content/renderer/media/android/stream_texture_factory_android_synchronous_impl.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/service/stream_texture_manager_in_process_android.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace content {

namespace {

using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;

int GetInProcessRendererId() {
  content::RenderProcessHost::iterator it =
      content::RenderProcessHost::AllHostsIterator();
  if (it.IsAtEnd()) {
    // There should always be one RPH in single process mode.
    NOTREACHED();
    return 0;
  }

  int id = it.GetCurrentValue()->GetID();
  it.Advance();
  DCHECK(it.IsAtEnd());  // Not multiprocess compatible.
  return id;
}

class VideoContextProvider
    : public StreamTextureFactorySynchronousImpl::ContextProvider {
 public:
  VideoContextProvider(
      const scoped_refptr<cc::ContextProvider>& context_provider,
      gpu::GLInProcessContext* gl_in_process_context)
      : context_provider_(context_provider),
        gl_in_process_context_(gl_in_process_context) {}

  virtual scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(
      uint32 stream_id) OVERRIDE {
    return gl_in_process_context_->GetSurfaceTexture(stream_id);
  }

  virtual blink::WebGraphicsContext3D* Context3d() OVERRIDE {
    return context_provider_->Context3d();
  }

 private:
  friend class base::RefCountedThreadSafe<VideoContextProvider>;
  virtual ~VideoContextProvider() {}

  scoped_refptr<cc::ContextProvider> context_provider_;
  gpu::GLInProcessContext* gl_in_process_context_;

  DISALLOW_COPY_AND_ASSIGN(VideoContextProvider);
};

class SynchronousCompositorFactoryImpl : public SynchronousCompositorFactory {
 public:
  SynchronousCompositorFactoryImpl()
      : wrapped_gl_context_for_main_thread_(NULL),
        num_hardware_compositors_(0) {
    SynchronousCompositorFactory::SetInstance(this);
  }

  // SynchronousCompositorFactory
  virtual scoped_refptr<base::MessageLoopProxy>
      GetCompositorMessageLoop() OVERRIDE {
    return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  }

  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      int routing_id) OVERRIDE {
    scoped_ptr<SynchronousCompositorOutputSurface> output_surface(
        new SynchronousCompositorOutputSurface(routing_id));
    return output_surface.PassAs<cc::OutputSurface>();
  }

  virtual InputHandlerManagerClient* GetInputHandlerManagerClient() OVERRIDE {
    return synchronous_input_event_filter();
  }

  SynchronousInputEventFilter* synchronous_input_event_filter() {
    return &synchronous_input_event_filter_;
  }

  scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
  CreateOffscreenContext() {
    if (!gfx::GLSurface::InitializeOneOff())
      return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>();

    const gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

    blink::WebGraphicsContext3D::Attributes attributes;
    attributes.antialias = false;
    attributes.shareResources = true;
    attributes.noAutomaticFlushes = true;

    gpu::GLInProcessContextAttribs in_process_attribs;
    WebGraphicsContext3DInProcessCommandBufferImpl::ConvertAttributes(
        attributes, &in_process_attribs);
    scoped_ptr<gpu::GLInProcessContext> context(
        gpu::GLInProcessContext::CreateContext(true,
                                               NULL,
                                               gfx::Size(1, 1),
                                               attributes.shareResources,
                                               in_process_attribs,
                                               gpu_preference));

    wrapped_gl_context_for_main_thread_ = context.get();
    if (!context.get())
      return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>();

    return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>(
        WebGraphicsContext3DInProcessCommandBufferImpl::WrapContext(
            context.Pass(), attributes));
  }

  virtual scoped_refptr<cc::ContextProvider>
  GetOffscreenContextProviderForMainThread() OVERRIDE {
    // This check only guarantees the main thread context is created after
    // a compositor did successfully initialize hardware draw in the past.
    // In particular this does not guarantee that the main thread context
    // will fail creation when all compositors release hardware draw.
    bool failed = !CanCreateMainThreadContext();
    if (!failed &&
        (!offscreen_context_for_main_thread_.get() ||
         offscreen_context_for_main_thread_->DestroyedOnMainThread())) {
      offscreen_context_for_main_thread_ =
          webkit::gpu::ContextProviderInProcess::Create(
              CreateOffscreenContext(),
              "Compositor-Offscreen");
      failed = !offscreen_context_for_main_thread_.get() ||
               !offscreen_context_for_main_thread_->BindToCurrentThread();
    }

    if (failed) {
      offscreen_context_for_main_thread_ = NULL;
      wrapped_gl_context_for_main_thread_ = NULL;
    }
    return offscreen_context_for_main_thread_;
  }

  // This is called on both renderer main thread (offscreen context creation
  // path shared between cross-process and in-process platforms) and renderer
  // compositor impl thread (InitializeHwDraw) in order to support Android
  // WebView synchronously enable and disable hardware mode multiple times in
  // the same task. This is ok because in-process WGC3D creation may happen on
  // any thread and is lightweight.
  virtual scoped_refptr<cc::ContextProvider>
      GetOffscreenContextProviderForCompositorThread() OVERRIDE {
    base::AutoLock lock(offscreen_context_for_compositor_thread_lock_);
    if (!offscreen_context_for_compositor_thread_.get() ||
        offscreen_context_for_compositor_thread_->DestroyedOnMainThread()) {
      offscreen_context_for_compositor_thread_ =
          webkit::gpu::ContextProviderInProcess::CreateOffscreen();
    }
    return offscreen_context_for_compositor_thread_;
  }

  virtual scoped_ptr<StreamTextureFactory> CreateStreamTextureFactory(
      int view_id) OVERRIDE {
    scoped_ptr<StreamTextureFactorySynchronousImpl> factory(
        new StreamTextureFactorySynchronousImpl(
            base::Bind(&SynchronousCompositorFactoryImpl::
                            TryCreateStreamTextureFactory,
                       base::Unretained(this)),
            view_id));
    return factory.PassAs<StreamTextureFactory>();
  }

  void CompositorInitializedHardwareDraw(SynchronousCompositorImpl* compositor);
  void CompositorReleasedHardwareDraw(SynchronousCompositorImpl* compositor);

 private:
  void ReleaseGlobalHardwareResources();
  bool CanCreateMainThreadContext();
  scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
      TryCreateStreamTextureFactory();

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

void SynchronousCompositorFactoryImpl::CompositorInitializedHardwareDraw(
    SynchronousCompositorImpl* compositor) {
  base::AutoLock lock(num_hardware_compositor_lock_);
  num_hardware_compositors_++;
}

void SynchronousCompositorFactoryImpl::CompositorReleasedHardwareDraw(
    SynchronousCompositorImpl* compositor) {
  bool should_release_resources = false;
  {
    base::AutoLock lock(num_hardware_compositor_lock_);
    DCHECK_GT(num_hardware_compositors_, 0u);
    num_hardware_compositors_--;
    should_release_resources = num_hardware_compositors_ == 0u;
  }
  if (should_release_resources)
    ReleaseGlobalHardwareResources();
}

void SynchronousCompositorFactoryImpl::ReleaseGlobalHardwareResources() {
  {
    base::AutoLock lock(offscreen_context_for_compositor_thread_lock_);
    offscreen_context_for_compositor_thread_ = NULL;
  }

  // TODO(boliu): Properly clean up command buffer server of main thread
  // context here.
}

bool SynchronousCompositorFactoryImpl::CanCreateMainThreadContext() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  return num_hardware_compositors_ > 0;
}

scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
SynchronousCompositorFactoryImpl::TryCreateStreamTextureFactory() {
  scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
      context_provider;
  if (CanCreateMainThreadContext() && offscreen_context_for_main_thread_) {
    DCHECK(wrapped_gl_context_for_main_thread_);
    context_provider =
        new VideoContextProvider(offscreen_context_for_main_thread_,
                                 wrapped_gl_context_for_main_thread_);
  }
  return context_provider;
}

base::LazyInstance<SynchronousCompositorFactoryImpl>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SynchronousCompositorImpl);

// static
SynchronousCompositorImpl* SynchronousCompositorImpl::FromID(int process_id,
                                                             int routing_id) {
  if (g_factory == NULL)
    return NULL;
  RenderViewHost* rvh = RenderViewHost::FromID(process_id, routing_id);
  if (!rvh)
    return NULL;
  WebContents* contents = WebContents::FromRenderViewHost(rvh);
  if (!contents)
    return NULL;
  return FromWebContents(contents);
}

SynchronousCompositorImpl* SynchronousCompositorImpl::FromRoutingID(
    int routing_id) {
  return FromID(GetInProcessRendererId(), routing_id);
}

SynchronousCompositorImpl::SynchronousCompositorImpl(WebContents* contents)
    : compositor_client_(NULL),
      output_surface_(NULL),
      contents_(contents),
      input_handler_(NULL) {
  DCHECK(contents);
}

SynchronousCompositorImpl::~SynchronousCompositorImpl() {
  if (compositor_client_)
    compositor_client_->DidDestroyCompositor(this);
  SetInputHandler(NULL);
}

void SynchronousCompositorImpl::SetClient(
    SynchronousCompositorClient* compositor_client) {
  DCHECK(CalledOnValidThread());
  compositor_client_ = compositor_client;
}

bool SynchronousCompositorImpl::InitializeHwDraw(
    scoped_refptr<gfx::GLSurface> surface) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  bool success = output_surface_->InitializeHwDraw(
      surface,
      g_factory.Get().GetOffscreenContextProviderForCompositorThread());
  if (success)
    g_factory.Get().CompositorInitializedHardwareDraw(this);
  return success;
}

void SynchronousCompositorImpl::ReleaseHwDraw() {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  output_surface_->ReleaseHwDraw();
  g_factory.Get().CompositorReleasedHardwareDraw(this);
}

bool SynchronousCompositorImpl::DemandDrawHw(
      gfx::Size surface_size,
      const gfx::Transform& transform,
      gfx::Rect viewport,
      gfx::Rect clip,
      bool stencil_enabled) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  return output_surface_->DemandDrawHw(
      surface_size, transform, viewport, clip, stencil_enabled);
}

bool SynchronousCompositorImpl::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  return output_surface_->DemandDrawSw(canvas);
}

void SynchronousCompositorImpl::SetMemoryPolicy(
    const SynchronousCompositorMemoryPolicy& policy) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  return output_surface_->SetMemoryPolicy(policy);
}

void SynchronousCompositorImpl::DidChangeRootLayerScrollOffset() {
  if (input_handler_)
    input_handler_->OnRootLayerDelegatedScrollOffsetChanged();
}

void SynchronousCompositorImpl::DidBindOutputSurface(
      SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(CalledOnValidThread());
  output_surface_ = output_surface;
  if (compositor_client_)
    compositor_client_->DidInitializeCompositor(this);
}

void SynchronousCompositorImpl::DidDestroySynchronousOutputSurface(
       SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(CalledOnValidThread());

  // Allow for transient hand-over when two output surfaces may refer to
  // a single delegate.
  if (output_surface_ == output_surface) {
    output_surface_ = NULL;
    if (compositor_client_)
      compositor_client_->DidDestroyCompositor(this);
    compositor_client_ = NULL;
  }
}

void SynchronousCompositorImpl::SetInputHandler(
    cc::InputHandler* input_handler) {
  DCHECK(CalledOnValidThread());

  if (input_handler_)
    input_handler_->SetRootLayerScrollOffsetDelegate(NULL);

  input_handler_ = input_handler;

  if (input_handler_)
    input_handler_->SetRootLayerScrollOffsetDelegate(this);
}

void SynchronousCompositorImpl::DidOverscroll(
    const cc::DidOverscrollParams& params) {
  if (compositor_client_) {
    compositor_client_->DidOverscroll(params.accumulated_overscroll,
                                      params.latest_overscroll_delta,
                                      params.current_fling_velocity);
  }
}

void SynchronousCompositorImpl::SetContinuousInvalidate(bool enable) {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    compositor_client_->SetContinuousInvalidate(enable);
}

InputEventAckState SynchronousCompositorImpl::HandleInputEvent(
    const blink::WebInputEvent& input_event) {
  DCHECK(CalledOnValidThread());
  return g_factory.Get().synchronous_input_event_filter()->HandleInputEvent(
      contents_->GetRoutingID(), input_event);
}

void SynchronousCompositorImpl::UpdateFrameMetaData(
    const cc::CompositorFrameMetadata& frame_metadata) {
  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      contents_->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->SynchronousFrameMetadata(frame_metadata);
}

void SynchronousCompositorImpl::DidActivatePendingTree() {
  if (compositor_client_)
    compositor_client_->DidUpdateContent();
}

void SynchronousCompositorImpl::SetMaxScrollOffset(
    gfx::Vector2dF max_scroll_offset) {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    compositor_client_->SetMaxRootLayerScrollOffset(max_scroll_offset);
}

void SynchronousCompositorImpl::SetTotalScrollOffset(gfx::Vector2dF new_value) {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    compositor_client_->SetTotalRootLayerScrollOffset(new_value);
}

gfx::Vector2dF SynchronousCompositorImpl::GetTotalScrollOffset() {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    return compositor_client_->GetTotalRootLayerScrollOffset();
  return gfx::Vector2dF();
}

bool SynchronousCompositorImpl::IsExternalFlingActive() const {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    return compositor_client_->IsExternalFlingActive();
  return false;
}

void SynchronousCompositorImpl::SetTotalPageScaleFactor(
    float page_scale_factor) {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    compositor_client_->SetRootLayerPageScaleFactor(page_scale_factor);
}

void SynchronousCompositorImpl::SetScrollableSize(gfx::SizeF scrollable_size) {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    compositor_client_->SetRootLayerScrollableSize(scrollable_size);
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorImpl() must only be used on the UI thread.
bool SynchronousCompositorImpl::CalledOnValidThread() const {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

// static
void SynchronousCompositor::SetClientForWebContents(
    WebContents* contents,
    SynchronousCompositorClient* client) {
  DCHECK(contents);
  if (client) {
    g_factory.Get();  // Ensure it's initialized.
    SynchronousCompositorImpl::CreateForWebContents(contents);
  }
  if (SynchronousCompositorImpl* instance =
      SynchronousCompositorImpl::FromWebContents(contents)) {
    instance->SetClient(client);
  }
}

}  // namespace content
