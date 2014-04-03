// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_process_transport_factory.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/browser_compositor_output_surface_proxy.h"
#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/software_browser_compositor_output_surface.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_constants.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include "content/browser/compositor/software_output_device_win.h"
#elif defined(USE_OZONE)
#include "content/browser/compositor/overlay_candidate_validator_ozone.h"
#include "content/browser/compositor/software_output_device_ozone.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"
#elif defined(USE_X11)
#include "content/browser/compositor/software_output_device_x11.h"
#endif

using cc::ContextProvider;
using gpu::gles2::GLES2Interface;

namespace content {

struct GpuProcessTransportFactory::PerCompositorData {
  int surface_id;
  scoped_refptr<ReflectorImpl> reflector;
};

class OwnedTexture : public ui::Texture, ImageTransportFactoryObserver {
 public:
  OwnedTexture(const scoped_refptr<ContextProvider>& provider,
               const gfx::Size& size,
               float device_scale_factor,
               GLuint texture_id)
      : ui::Texture(true, size, device_scale_factor),
        provider_(provider),
        texture_id_(texture_id) {
    ImageTransportFactory::GetInstance()->AddObserver(this);
  }

  // ui::Texture overrides:
  virtual unsigned int PrepareTexture() OVERRIDE {
    // It's possible that we may have lost the context owning our
    // texture but not yet fired the OnLostResources callback, so poll to see if
    // it's still valid.
    if (provider_ && provider_->IsContextLost())
      texture_id_ = 0u;
    return texture_id_;
  }

  // ImageTransportFactory overrides:
  virtual void OnLostResources() OVERRIDE {
    DeleteTexture();
    provider_ = NULL;
  }

 protected:
  virtual ~OwnedTexture() {
    ImageTransportFactory::GetInstance()->RemoveObserver(this);
    DeleteTexture();
  }

 protected:
  void DeleteTexture() {
    if (texture_id_) {
      provider_->ContextGL()->DeleteTextures(1, &texture_id_);
      texture_id_ = 0;
    }
  }

  scoped_refptr<cc::ContextProvider> provider_;
  GLuint texture_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OwnedTexture);
};

class ImageTransportClientTexture : public OwnedTexture {
 public:
  ImageTransportClientTexture(const scoped_refptr<ContextProvider>& provider,
                              float device_scale_factor,
                              GLuint texture_id)
      : OwnedTexture(provider,
                     gfx::Size(0, 0),
                     device_scale_factor,
                     texture_id) {}

  virtual void Consume(const gpu::Mailbox& mailbox,
                       const gfx::Size& new_size) OVERRIDE {
    mailbox_ = mailbox;
    if (mailbox.IsZero())
      return;

    DCHECK(provider_ && texture_id_);
    GLES2Interface* gl = provider_->ContextGL();
    gl->BindTexture(GL_TEXTURE_2D, texture_id_);
    gl->ConsumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    size_ = new_size;
    gl->ShallowFlushCHROMIUM();
  }

  virtual gpu::Mailbox Produce() OVERRIDE { return mailbox_; }

 protected:
  virtual ~ImageTransportClientTexture() {}

 private:
  gpu::Mailbox mailbox_;
  DISALLOW_COPY_AND_ASSIGN(ImageTransportClientTexture);
};

GpuProcessTransportFactory::GpuProcessTransportFactory()
    : callback_factory_(this), offscreen_content_bound_to_other_thread_(false) {
  output_surface_proxy_ = new BrowserCompositorOutputSurfaceProxy(
      &output_surface_map_);
}

GpuProcessTransportFactory::~GpuProcessTransportFactory() {
  DCHECK(per_compositor_data_.empty());

  // Make sure the lost context callback doesn't try to run during destruction.
  callback_factory_.InvalidateWeakPtrs();

  if (offscreen_compositor_contexts_.get() &&
      offscreen_content_bound_to_other_thread_) {
    // Leak shared contexts on other threads, as we can not get to the correct
    // thread to destroy them.
    offscreen_compositor_contexts_->set_leak_on_destroy();
  }
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
GpuProcessTransportFactory::CreateOffscreenCommandBufferContext() {
  return CreateContextCommon(0);
}

scoped_ptr<cc::SoftwareOutputDevice> CreateSoftwareOutputDevice(
    ui::Compositor* compositor) {
#if defined(OS_WIN)
  return scoped_ptr<cc::SoftwareOutputDevice>(new SoftwareOutputDeviceWin(
      compositor));
#elif defined(USE_OZONE)
  return scoped_ptr<cc::SoftwareOutputDevice>(new SoftwareOutputDeviceOzone(
      compositor));
#elif defined(USE_X11)
  return scoped_ptr<cc::SoftwareOutputDevice>(new SoftwareOutputDeviceX11(
      compositor));
#else
  NOTREACHED();
  return scoped_ptr<cc::SoftwareOutputDevice>();
#endif
}

scoped_ptr<cc::OverlayCandidateValidator> CreateOverlayCandidateValidator(
    gfx::AcceleratedWidget widget) {
#if defined(USE_OZONE)
  gfx::OverlayCandidatesOzone* overlay_candidates =
      gfx::SurfaceFactoryOzone::GetInstance()->GetOverlayCandidates(widget);
  if (overlay_candidates && CommandLine::ForCurrentProcess()->HasSwitch(
                                switches::kEnableHardwareOverlays)) {
    return scoped_ptr<cc::OverlayCandidateValidator>(
        new OverlayCandidateValidatorOzone(widget, overlay_candidates));
  }
#endif
  return scoped_ptr<cc::OverlayCandidateValidator>();
}

scoped_ptr<cc::OutputSurface> GpuProcessTransportFactory::CreateOutputSurface(
    ui::Compositor* compositor, bool software_fallback) {
  PerCompositorData* data = per_compositor_data_[compositor];
  if (!data)
    data = CreatePerCompositorData(compositor);

  bool create_software_renderer = software_fallback;
#if defined(OS_CHROMEOS)
  // Software fallback does not happen on Chrome OS.
  create_software_renderer = false;
#elif defined(OS_WIN)
  if (::GetProp(compositor->widget(), kForceSoftwareCompositor)) {
    if (::RemoveProp(compositor->widget(), kForceSoftwareCompositor))
      create_software_renderer = true;
  }
#endif

  scoped_refptr<ContextProviderCommandBuffer> context_provider;
  if (!create_software_renderer) {
    context_provider = ContextProviderCommandBuffer::Create(
        GpuProcessTransportFactory::CreateContextCommon(data->surface_id),
        "Compositor");
  }

  UMA_HISTOGRAM_BOOLEAN("Aura.CreatedGpuBrowserCompositor", !!context_provider);

  if (!context_provider.get()) {
    if (ui::Compositor::WasInitializedWithThread()) {
      LOG(FATAL) << "Failed to create UI context, but can't use software"
                 " compositing with browser threaded compositing. Aborting.";
    }

    scoped_ptr<SoftwareBrowserCompositorOutputSurface> surface(
        new SoftwareBrowserCompositorOutputSurface(
            output_surface_proxy_,
            CreateSoftwareOutputDevice(compositor),
            per_compositor_data_[compositor]->surface_id,
            &output_surface_map_,
            compositor->vsync_manager()));
    return surface.PassAs<cc::OutputSurface>();
  }

  scoped_refptr<base::SingleThreadTaskRunner> compositor_thread_task_runner =
      ui::Compositor::GetCompositorMessageLoop();
  if (!compositor_thread_task_runner.get())
    compositor_thread_task_runner = base::MessageLoopProxy::current();

  // Here we know the GpuProcessHost has been set up, because we created a
  // context.
  output_surface_proxy_->ConnectToGpuProcessHost(
      compositor_thread_task_runner.get());

  scoped_ptr<BrowserCompositorOutputSurface> surface(
      new GpuBrowserCompositorOutputSurface(
          context_provider,
          per_compositor_data_[compositor]->surface_id,
          &output_surface_map_,
          compositor->vsync_manager(),
          CreateOverlayCandidateValidator(compositor->widget())));
  if (data->reflector.get())
    data->reflector->ReattachToOutputSurfaceFromMainThread(surface.get());

  return surface.PassAs<cc::OutputSurface>();
}

scoped_refptr<ui::Reflector> GpuProcessTransportFactory::CreateReflector(
    ui::Compositor* source,
    ui::Layer* target) {
  PerCompositorData* data = per_compositor_data_[source];
  DCHECK(data);

  if (data->reflector.get())
    RemoveObserver(data->reflector.get());

  data->reflector = new ReflectorImpl(
      source, target, &output_surface_map_, data->surface_id);
  AddObserver(data->reflector.get());
  return data->reflector;
}

void GpuProcessTransportFactory::RemoveReflector(
    scoped_refptr<ui::Reflector> reflector) {
  ReflectorImpl* reflector_impl =
      static_cast<ReflectorImpl*>(reflector.get());
  PerCompositorData* data =
      per_compositor_data_[reflector_impl->mirrored_compositor()];
  DCHECK(data);
  RemoveObserver(reflector_impl);
  data->reflector->Shutdown();
  data->reflector = NULL;
}

void GpuProcessTransportFactory::RemoveCompositor(ui::Compositor* compositor) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second;
  DCHECK(data);
  GpuSurfaceTracker::Get()->RemoveSurface(data->surface_id);
  delete data;
  per_compositor_data_.erase(it);
  if (per_compositor_data_.empty()) {
    // Destroying the GLHelper may cause some async actions to be cancelled,
    // causing things to request a new GLHelper. Due to crbug.com/176091 the
    // GLHelper created in this case would be lost/leaked if we just reset()
    // on the |gl_helper_| variable directly. So instead we call reset() on a
    // local scoped_ptr.
    scoped_ptr<GLHelper> helper = gl_helper_.Pass();
    helper.reset();
    DCHECK(!gl_helper_) << "Destroying the GLHelper should not cause a new "
                           "GLHelper to be created.";
  }
}

bool GpuProcessTransportFactory::DoesCreateTestContexts() { return false; }

ui::ContextFactory* GpuProcessTransportFactory::AsContextFactory() {
  return this;
}

gfx::GLSurfaceHandle GpuProcessTransportFactory::GetSharedSurfaceHandle() {
  gfx::GLSurfaceHandle handle = gfx::GLSurfaceHandle(
      gfx::kNullPluginWindow, gfx::TEXTURE_TRANSPORT);
  handle.parent_client_id =
      BrowserGpuChannelHostFactory::instance()->GetGpuChannelId();
  return handle;
}

scoped_refptr<ui::Texture> GpuProcessTransportFactory::CreateTransportClient(
    float device_scale_factor) {
  scoped_refptr<cc::ContextProvider> provider =
      SharedMainThreadContextProvider();
  if (!provider.get())
    return NULL;
  GLuint texture_id = 0;
  provider->ContextGL()->GenTextures(1, &texture_id);
  scoped_refptr<ImageTransportClientTexture> image(
      new ImageTransportClientTexture(
          provider, device_scale_factor, texture_id));
  return image;
}

scoped_refptr<ui::Texture> GpuProcessTransportFactory::CreateOwnedTexture(
    const gfx::Size& size,
    float device_scale_factor,
    unsigned int texture_id) {
  scoped_refptr<cc::ContextProvider> provider =
      SharedMainThreadContextProvider();
  if (!provider.get())
    return NULL;
  scoped_refptr<OwnedTexture> image(new OwnedTexture(
      provider, size, device_scale_factor, texture_id));
  return image;
}

GLHelper* GpuProcessTransportFactory::GetGLHelper() {
  if (!gl_helper_ && !per_compositor_data_.empty()) {
    scoped_refptr<cc::ContextProvider> provider =
        SharedMainThreadContextProvider();
    if (provider.get())
      gl_helper_.reset(new GLHelper(provider->ContextGL(),
                                    provider->ContextSupport()));
  }
  return gl_helper_.get();
}

void GpuProcessTransportFactory::AddObserver(
    ImageTransportFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void GpuProcessTransportFactory::RemoveObserver(
    ImageTransportFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

scoped_refptr<cc::ContextProvider>
GpuProcessTransportFactory::OffscreenCompositorContextProvider() {
  // Don't check for DestroyedOnMainThread() here. We hear about context
  // loss for this context through the lost context callback. If the context
  // is lost, we want to leave this ContextProvider available until the lost
  // context notification is sent to the ImageTransportFactoryObserver clients.
  if (offscreen_compositor_contexts_.get())
    return offscreen_compositor_contexts_;

  offscreen_compositor_contexts_ = ContextProviderCommandBuffer::Create(
      GpuProcessTransportFactory::CreateOffscreenCommandBufferContext(),
      "Compositor-Offscreen");
  offscreen_content_bound_to_other_thread_ =
      ui::Compositor::WasInitializedWithThread();

  return offscreen_compositor_contexts_;
}

scoped_refptr<cc::ContextProvider>
GpuProcessTransportFactory::SharedMainThreadContextProvider() {
  if (shared_main_thread_contexts_.get())
    return shared_main_thread_contexts_;

  // In threaded compositing mode, we have to create our own context for the
  // main thread since the compositor's context will be bound to the
  // compositor thread. When not in threaded mode, we still need a separate
  // context so that skia and gl_helper don't step on each other.
  shared_main_thread_contexts_ = ContextProviderCommandBuffer::Create(
      GpuProcessTransportFactory::CreateOffscreenCommandBufferContext(),
      "Offscreen-MainThread");

  if (shared_main_thread_contexts_) {
    shared_main_thread_contexts_->SetLostContextCallback(
        base::Bind(&GpuProcessTransportFactory::
                        OnLostMainThreadSharedContextInsideCallback,
                   callback_factory_.GetWeakPtr()));
    if (!shared_main_thread_contexts_->BindToCurrentThread()) {
      shared_main_thread_contexts_ = NULL;
      offscreen_compositor_contexts_ = NULL;
    }
  }
  return shared_main_thread_contexts_;
}

GpuProcessTransportFactory::PerCompositorData*
GpuProcessTransportFactory::CreatePerCompositorData(
    ui::Compositor* compositor) {
  DCHECK(!per_compositor_data_[compositor]);

  gfx::AcceleratedWidget widget = compositor->widget();
  GpuSurfaceTracker* tracker = GpuSurfaceTracker::Get();

  PerCompositorData* data = new PerCompositorData;
  data->surface_id = tracker->AddSurfaceForNativeWidget(widget);
  tracker->SetSurfaceHandle(
      data->surface_id,
      gfx::GLSurfaceHandle(widget, gfx::NATIVE_DIRECT));

  per_compositor_data_[compositor] = data;

  return data;
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
GpuProcessTransportFactory::CreateContextCommon(int surface_id) {
  if (!GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor())
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  attrs.depth = false;
  attrs.stencil = false;
  attrs.antialias = false;
  attrs.noAutomaticFlushes = true;
  bool lose_context_when_out_of_memory = true;
  CauseForGpuLaunch cause =
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
  scoped_refptr<GpuChannelHost> gpu_channel_host(
      BrowserGpuChannelHostFactory::instance()->EstablishGpuChannelSync(cause));
  if (!gpu_channel_host) {
    LOG(ERROR) << "Failed to establish GPU channel.";
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  }
  GURL url("chrome://gpu/GpuProcessTransportFactory::CreateContextCommon");
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(
          surface_id,
          url,
          gpu_channel_host.get(),
          attrs,
          lose_context_when_out_of_memory,
          WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits(),
          NULL));
  return context.Pass();
}

void GpuProcessTransportFactory::OnLostMainThreadSharedContextInsideCallback() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GpuProcessTransportFactory::OnLostMainThreadSharedContext,
                 callback_factory_.GetWeakPtr()));
}

void GpuProcessTransportFactory::OnLostMainThreadSharedContext() {
  LOG(ERROR) << "Lost UI shared context.";

  // Keep old resources around while we call the observers, but ensure that
  // new resources are created if needed.
  // Kill shared contexts for both threads in tandem so they are always in
  // the same share group.
  scoped_refptr<cc::ContextProvider> lost_offscreen_compositor_contexts =
      offscreen_compositor_contexts_;
  scoped_refptr<cc::ContextProvider> lost_shared_main_thread_contexts =
      shared_main_thread_contexts_;
  offscreen_compositor_contexts_ = NULL;
  shared_main_thread_contexts_  = NULL;

  scoped_ptr<GLHelper> lost_gl_helper = gl_helper_.Pass();

  FOR_EACH_OBSERVER(ImageTransportFactoryObserver,
                    observer_list_,
                    OnLostResources());

  // Kill things that use the shared context before killing the shared context.
  lost_gl_helper.reset();
  lost_offscreen_compositor_contexts = NULL;
  lost_shared_main_thread_contexts  = NULL;
}

}  // namespace content
