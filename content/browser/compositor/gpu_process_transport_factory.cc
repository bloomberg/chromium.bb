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
#include "base/threading/thread.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/surfaces/onscreen_display_client.h"
#include "cc/surfaces/surface_display_output_surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"
#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/software_browser_compositor_output_surface.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/public/common/content_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_constants.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_WIN)
#include "content/browser/compositor/software_output_device_win.h"
#elif defined(USE_OZONE)
#include "content/browser/compositor/overlay_candidate_validator_ozone.h"
#include "content/browser/compositor/software_output_device_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#elif defined(USE_X11)
#include "content/browser/compositor/software_output_device_x11.h"
#elif defined(OS_MACOSX)
#include "content/browser/compositor/software_output_device_mac.h"
#endif

using cc::ContextProvider;
using gpu::gles2::GLES2Interface;

static const int kNumRetriesBeforeSoftwareFallback = 4;

namespace content {

struct GpuProcessTransportFactory::PerCompositorData {
  int surface_id;
  BrowserCompositorOutputSurface* surface;
  ReflectorImpl* reflector;
  scoped_ptr<cc::OnscreenDisplayClient> display_client;

  PerCompositorData() : surface_id(0), surface(nullptr), reflector(nullptr) {}
};

GpuProcessTransportFactory::GpuProcessTransportFactory()
    : next_surface_id_namespace_(1u),
      callback_factory_(this) {
  if (UseSurfacesEnabled())
    surface_manager_ = make_scoped_ptr(new cc::SurfaceManager);
}

GpuProcessTransportFactory::~GpuProcessTransportFactory() {
  DCHECK(per_compositor_data_.empty());

  // Make sure the lost context callback doesn't try to run during destruction.
  callback_factory_.InvalidateWeakPtrs();
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
GpuProcessTransportFactory::CreateOffscreenCommandBufferContext() {
  CauseForGpuLaunch cause =
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
  scoped_refptr<GpuChannelHost> gpu_channel_host(
      BrowserGpuChannelHostFactory::instance()->EstablishGpuChannelSync(cause));
  return CreateContextCommon(gpu_channel_host, 0);
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
#elif defined(OS_MACOSX)
  return scoped_ptr<cc::SoftwareOutputDevice>(
      new SoftwareOutputDeviceMac(compositor));
#else
  NOTREACHED();
  return scoped_ptr<cc::SoftwareOutputDevice>();
#endif
}

scoped_ptr<cc::OverlayCandidateValidator> CreateOverlayCandidateValidator(
    gfx::AcceleratedWidget widget) {
#if defined(USE_OZONE)
  ui::OverlayCandidatesOzone* overlay_candidates =
      ui::SurfaceFactoryOzone::GetInstance()->GetOverlayCandidates(widget);
  if (overlay_candidates &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableHardwareOverlays)) {
    return scoped_ptr<cc::OverlayCandidateValidator>(
        new OverlayCandidateValidatorOzone(widget, overlay_candidates));
  }
#endif
  return scoped_ptr<cc::OverlayCandidateValidator>();
}

static bool ShouldCreateGpuOutputSurface(ui::Compositor* compositor) {
#if defined(OS_CHROMEOS)
  // Software fallback does not happen on Chrome OS.
  return true;
#endif

#if defined(OS_WIN)
  if (::GetProp(compositor->widget(), kForceSoftwareCompositor) &&
      ::RemoveProp(compositor->widget(), kForceSoftwareCompositor))
    return false;
#endif

  return GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor();
}

void GpuProcessTransportFactory::CreateOutputSurface(
    base::WeakPtr<ui::Compositor> compositor) {
  DCHECK(!!compositor);
  PerCompositorData* data = per_compositor_data_[compositor.get()];
  if (!data) {
    data = CreatePerCompositorData(compositor.get());
  } else {
    // TODO(piman): Use GpuSurfaceTracker to map ids to surfaces instead of an
    // output_surface_map_ here.
    output_surface_map_.Remove(data->surface_id);
    data->surface = nullptr;
  }

  bool create_gpu_output_surface =
      ShouldCreateGpuOutputSurface(compositor.get());
  if (create_gpu_output_surface) {
    CauseForGpuLaunch cause =
        CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
    BrowserGpuChannelHostFactory::instance()->EstablishGpuChannel(
        cause, base::Bind(&GpuProcessTransportFactory::EstablishedGpuChannel,
                          callback_factory_.GetWeakPtr(), compositor,
                          create_gpu_output_surface, 0));
  } else {
    EstablishedGpuChannel(compositor, create_gpu_output_surface, 0);
  }
}

void GpuProcessTransportFactory::EstablishedGpuChannel(
    base::WeakPtr<ui::Compositor> compositor,
    bool create_gpu_output_surface,
    int num_attempts) {
  if (!compositor)
    return;
  PerCompositorData* data = per_compositor_data_[compositor.get()];
  DCHECK(data);

  if (num_attempts > kNumRetriesBeforeSoftwareFallback) {
#if defined(OS_CHROMEOS)
    LOG(FATAL) << "Unable to create a UI graphics context, and cannot use "
               << "software compositing on ChromeOS.";
#endif
    create_gpu_output_surface = false;
  }

  scoped_refptr<ContextProviderCommandBuffer> context_provider;
  if (create_gpu_output_surface) {
    scoped_refptr<GpuChannelHost> gpu_channel_host =
        BrowserGpuChannelHostFactory::instance()->GetGpuChannel();
    if (gpu_channel_host.get()) {
      context_provider = ContextProviderCommandBuffer::Create(
          GpuProcessTransportFactory::CreateContextCommon(gpu_channel_host,
                                                          data->surface_id),
          "Compositor");
      if (context_provider && !context_provider->BindToCurrentThread())
        context_provider = nullptr;
    }

    UMA_HISTOGRAM_BOOLEAN("Aura.CreatedGpuBrowserCompositor",
                          !!context_provider.get());

    if (!context_provider) {
      // Try again.
      CauseForGpuLaunch cause =
          CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
      BrowserGpuChannelHostFactory::instance()->EstablishGpuChannel(
          cause, base::Bind(&GpuProcessTransportFactory::EstablishedGpuChannel,
                            callback_factory_.GetWeakPtr(), compositor,
                            create_gpu_output_surface, num_attempts + 1));
      return;
    }
  }

  scoped_ptr<BrowserCompositorOutputSurface> surface;
  if (!create_gpu_output_surface) {
    surface = make_scoped_ptr(new SoftwareBrowserCompositorOutputSurface(
        CreateSoftwareOutputDevice(compositor.get()),
        compositor->vsync_manager()));
  } else {
    DCHECK(context_provider);
#if defined(USE_OZONE)
    if (ui::SurfaceFactoryOzone::GetInstance()
            ->CanShowPrimaryPlaneAsOverlay()) {
      surface =
          make_scoped_ptr(new GpuSurfacelessBrowserCompositorOutputSurface(
              context_provider, data->surface_id, compositor->vsync_manager(),
              CreateOverlayCandidateValidator(compositor->widget()), GL_RGB,
              BrowserGpuMemoryBufferManager::current()));
    } else
#endif
    {
      surface = make_scoped_ptr(new GpuBrowserCompositorOutputSurface(
          context_provider, compositor->vsync_manager(),
          CreateOverlayCandidateValidator(compositor->widget())));
    }
  }

  // TODO(piman): Use GpuSurfaceTracker to map ids to surfaces instead of an
  // output_surface_map_ here.
  output_surface_map_.AddWithID(surface.get(), data->surface_id);
  data->surface = surface.get();
  if (data->reflector)
    data->reflector->OnSourceSurfaceReady(data->surface);

  if (!UseSurfacesEnabled()) {
    compositor->SetOutputSurface(surface.Pass());
    return;
  }

  // This gets a bit confusing. Here we have a ContextProvider in the |surface|
  // configured to render directly to this widget. We need to make an
  // OnscreenDisplayClient associated with that context, then return a
  // SurfaceDisplayOutputSurface set up to draw to the display's surface.
  cc::SurfaceManager* manager = surface_manager_.get();
  scoped_ptr<cc::OnscreenDisplayClient> display_client(
      new cc::OnscreenDisplayClient(
          surface.Pass(), manager, HostSharedBitmapManager::current(),
          BrowserGpuMemoryBufferManager::current(),
          compositor->GetRendererSettings(), compositor->task_runner()));

  scoped_ptr<cc::SurfaceDisplayOutputSurface> output_surface(
      new cc::SurfaceDisplayOutputSurface(
          manager, compositor->surface_id_allocator(), context_provider));
  display_client->set_surface_output_surface(output_surface.get());
  output_surface->set_display_client(display_client.get());
  display_client->display()->Resize(compositor->size());
  data->display_client = display_client.Pass();
  compositor->SetOutputSurface(output_surface.Pass());
}

scoped_ptr<ui::Reflector> GpuProcessTransportFactory::CreateReflector(
    ui::Compositor* source_compositor,
    ui::Layer* target_layer) {
  PerCompositorData* source_data = per_compositor_data_[source_compositor];
  DCHECK(source_data);

  scoped_ptr<ReflectorImpl> reflector(
      new ReflectorImpl(source_compositor, target_layer));
  source_data->reflector = reflector.get();
  if (BrowserCompositorOutputSurface* source_surface = source_data->surface)
    reflector->OnSourceSurfaceReady(source_surface);
  return reflector.Pass();
}

void GpuProcessTransportFactory::RemoveReflector(ui::Reflector* reflector) {
  ReflectorImpl* reflector_impl = static_cast<ReflectorImpl*>(reflector);
  PerCompositorData* data =
      per_compositor_data_[reflector_impl->mirrored_compositor()];
  DCHECK(data);
  data->reflector->Shutdown();
  data->reflector = nullptr;
}

void GpuProcessTransportFactory::RemoveCompositor(ui::Compositor* compositor) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second;
  DCHECK(data);
  // TODO(piman): Use GpuSurfaceTracker to map ids to surfaces instead of an
  // output_surface_map_ here.
  if (data->surface)
    output_surface_map_.Remove(data->surface_id);
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

    // If there are any observer left at this point, make sure they clean up
    // before we destroy the GLHelper.
    FOR_EACH_OBSERVER(
        ImageTransportFactoryObserver, observer_list_, OnLostResources());

    helper.reset();
    DCHECK(!gl_helper_) << "Destroying the GLHelper should not cause a new "
                           "GLHelper to be created.";
  }
}

bool GpuProcessTransportFactory::DoesCreateTestContexts() { return false; }

uint32 GpuProcessTransportFactory::GetImageTextureTarget() {
  return BrowserGpuChannelHostFactory::GetImageTextureTarget();
}

cc::SharedBitmapManager* GpuProcessTransportFactory::GetSharedBitmapManager() {
  return HostSharedBitmapManager::current();
}

gpu::GpuMemoryBufferManager*
GpuProcessTransportFactory::GetGpuMemoryBufferManager() {
  return BrowserGpuMemoryBufferManager::current();
}

ui::ContextFactory* GpuProcessTransportFactory::GetContextFactory() {
  return this;
}

gfx::GLSurfaceHandle GpuProcessTransportFactory::GetSharedSurfaceHandle() {
  gfx::GLSurfaceHandle handle = gfx::GLSurfaceHandle(
      gfx::kNullPluginWindow, gfx::NULL_TRANSPORT);
  handle.parent_client_id =
      BrowserGpuChannelHostFactory::instance()->GetGpuChannelId();
  return handle;
}

scoped_ptr<cc::SurfaceIdAllocator>
GpuProcessTransportFactory::CreateSurfaceIdAllocator() {
  return make_scoped_ptr(
      new cc::SurfaceIdAllocator(next_surface_id_namespace_++));
}

void GpuProcessTransportFactory::ResizeDisplay(ui::Compositor* compositor,
                                               const gfx::Size& size) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second;
  DCHECK(data);
  if (data->display_client)
    data->display_client->display()->Resize(size);
}

cc::SurfaceManager* GpuProcessTransportFactory::GetSurfaceManager() {
  return surface_manager_.get();
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

#if defined(OS_MACOSX)
void GpuProcessTransportFactory::OnSurfaceDisplayed(int surface_id) {
  BrowserCompositorOutputSurface* surface = output_surface_map_.Lookup(
      surface_id);
  if (surface)
    surface->OnSurfaceDisplayed();
}

void GpuProcessTransportFactory::OnCompositorRecycled(
    ui::Compositor* compositor) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second;
  DCHECK(data);
  BrowserCompositorOutputSurface* surface =
      output_surface_map_.Lookup(data->surface_id);
  if (surface)
    surface->OnSurfaceRecycled();
}

bool GpuProcessTransportFactory::SurfaceShouldNotShowFramesAfterRecycle(
    int surface_id) const {
  BrowserCompositorOutputSurface* surface =
      output_surface_map_.Lookup(surface_id);
  if (surface)
    return surface->ShouldNotShowFramesAfterRecycle();
  return false;
}
#endif

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

  if (shared_main_thread_contexts_.get()) {
    shared_main_thread_contexts_->SetLostContextCallback(
        base::Bind(&GpuProcessTransportFactory::
                        OnLostMainThreadSharedContextInsideCallback,
                   callback_factory_.GetWeakPtr()));
    if (!shared_main_thread_contexts_->BindToCurrentThread())
      shared_main_thread_contexts_ = NULL;
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
GpuProcessTransportFactory::CreateContextCommon(
    scoped_refptr<GpuChannelHost> gpu_channel_host,
    int surface_id) {
  if (!GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor())
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  attrs.depth = false;
  attrs.stencil = false;
  attrs.antialias = false;
  attrs.noAutomaticFlushes = true;
  bool lose_context_when_out_of_memory = true;
  if (!gpu_channel_host.get()) {
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
  scoped_refptr<cc::ContextProvider> lost_shared_main_thread_contexts =
      shared_main_thread_contexts_;
  shared_main_thread_contexts_  = NULL;

  scoped_ptr<GLHelper> lost_gl_helper = gl_helper_.Pass();

  FOR_EACH_OBSERVER(ImageTransportFactoryObserver,
                    observer_list_,
                    OnLostResources());

  // Kill things that use the shared context before killing the shared context.
  lost_gl_helper.reset();
  lost_shared_main_thread_contexts  = NULL;
}

}  // namespace content
