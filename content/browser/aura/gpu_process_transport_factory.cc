// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aura/gpu_process_transport_factory.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "content/browser/aura/browser_compositor_output_surface.h"
#include "content/browser/aura/browser_compositor_output_surface_proxy.h"
#include "content/browser/aura/reflector_impl.h"
#include "content/browser/aura/software_browser_compositor_output_surface.h"
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
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include "content/browser/aura/software_output_device_win.h"
#include "ui/surface/accelerated_surface_win.h"
#elif defined(USE_X11)
#include "content/browser/aura/software_output_device_x11.h"
#endif

namespace content {

struct GpuProcessTransportFactory::PerCompositorData {
  int surface_id;
  scoped_ptr<CompositorSwapClient> swap_client;
#if defined(OS_WIN)
  scoped_ptr<AcceleratedSurface> accelerated_surface;
#endif
  scoped_refptr<ReflectorImpl> reflector;
};

class OwnedTexture : public ui::Texture, ImageTransportFactoryObserver {
 public:
  OwnedTexture(WebKit::WebGraphicsContext3D* host_context,
               const gfx::Size& size,
               float device_scale_factor,
               unsigned int texture_id)
      : ui::Texture(true, size, device_scale_factor),
        host_context_(host_context),
        texture_id_(texture_id) {
    ImageTransportFactory::GetInstance()->AddObserver(this);
  }

  // ui::Texture overrides:
  virtual unsigned int PrepareTexture() OVERRIDE {
    return texture_id_;
  }

  virtual WebKit::WebGraphicsContext3D* HostContext3D() OVERRIDE {
    return host_context_;
  }

  // ImageTransportFactory overrides:
  virtual void OnLostResources() OVERRIDE {
    DeleteTexture();
  }

 protected:
  virtual ~OwnedTexture() {
    ImageTransportFactory::GetInstance()->RemoveObserver(this);
    DeleteTexture();
  }

 protected:
  void DeleteTexture() {
    if (texture_id_) {
      host_context_->deleteTexture(texture_id_);
      texture_id_ = 0;
    }
  }

  // A raw pointer. This |ImageTransportClientTexture| will be destroyed
  // before the |host_context_| via
  // |ImageTransportFactoryObserver::OnLostContext()| handlers.
  WebKit::WebGraphicsContext3D* host_context_;
  unsigned texture_id_;

  DISALLOW_COPY_AND_ASSIGN(OwnedTexture);
};

class ImageTransportClientTexture : public OwnedTexture {
 public:
  ImageTransportClientTexture(
      WebKit::WebGraphicsContext3D* host_context,
      float device_scale_factor)
      : OwnedTexture(host_context,
                     gfx::Size(0, 0),
                     device_scale_factor,
                     host_context->createTexture()) {
  }

  virtual void Consume(const std::string& mailbox_name,
                       const gfx::Size& new_size) OVERRIDE {
    DCHECK(mailbox_name.size() == GL_MAILBOX_SIZE_CHROMIUM);
    mailbox_name_ = mailbox_name;
    if (mailbox_name.empty())
      return;

    DCHECK(host_context_ && texture_id_);
    host_context_->bindTexture(GL_TEXTURE_2D, texture_id_);
    host_context_->consumeTextureCHROMIUM(
        GL_TEXTURE_2D,
        reinterpret_cast<const signed char*>(mailbox_name.c_str()));
    size_ = new_size;
    host_context_->shallowFlushCHROMIUM();
  }

  virtual std::string Produce() OVERRIDE {
    return mailbox_name_;
  }

 protected:
  virtual ~ImageTransportClientTexture() {}

 private:
  std::string mailbox_name_;
  DISALLOW_COPY_AND_ASSIGN(ImageTransportClientTexture);
};

class CompositorSwapClient
    : public base::SupportsWeakPtr<CompositorSwapClient>,
      public WebGraphicsContext3DSwapBuffersClient {
 public:
  CompositorSwapClient(ui::Compositor* compositor,
                       GpuProcessTransportFactory* factory)
      : compositor_(compositor),
        factory_(factory) {
  }

  virtual ~CompositorSwapClient() {
  }

  virtual void OnViewContextSwapBuffersPosted() OVERRIDE {
    compositor_->OnSwapBuffersPosted();
  }

  virtual void OnViewContextSwapBuffersComplete() OVERRIDE {
    compositor_->OnSwapBuffersComplete();
  }

  virtual void OnViewContextSwapBuffersAborted() OVERRIDE {
    // Recreating contexts directly from here causes issues, so post a task
    // instead.
    // TODO(piman): Fix the underlying issues.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&CompositorSwapClient::OnLostContext, this->AsWeakPtr()));
  }

 private:
  void OnLostContext() {
    factory_->OnLostContext(compositor_);
    // Note: previous line destroyed this. Don't access members from now on.
  }

  ui::Compositor* compositor_;
  GpuProcessTransportFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorSwapClient);
};

GpuProcessTransportFactory::GpuProcessTransportFactory()
    : callback_factory_(this) {
  output_surface_proxy_ = new BrowserCompositorOutputSurfaceProxy(
      &output_surface_map_);
}

GpuProcessTransportFactory::~GpuProcessTransportFactory() {
  DCHECK(per_compositor_data_.empty());

  // Make sure the lost context callback doesn't try to run during destruction.
  callback_factory_.InvalidateWeakPtrs();
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
GpuProcessTransportFactory::CreateOffscreenCommandBufferContext() {
  base::WeakPtr<WebGraphicsContext3DSwapBuffersClient> swap_client;
  return CreateContextCommon(swap_client, 0);
}

scoped_ptr<WebKit::WebGraphicsContext3D>
GpuProcessTransportFactory::CreateOffscreenContext() {
  return CreateOffscreenCommandBufferContext()
      .PassAs<WebKit::WebGraphicsContext3D>();
}

scoped_ptr<cc::SoftwareOutputDevice> CreateSoftwareOutputDevice(
    ui::Compositor* compositor) {
#if defined(OS_WIN)
  return scoped_ptr<cc::SoftwareOutputDevice>(new SoftwareOutputDeviceWin(
      compositor));
#elif defined(USE_X11)
  return scoped_ptr<cc::SoftwareOutputDevice>(new SoftwareOutputDeviceX11(
      compositor));
#endif

  NOTREACHED();
  return scoped_ptr<cc::SoftwareOutputDevice>();
}

scoped_ptr<cc::OutputSurface> GpuProcessTransportFactory::CreateOutputSurface(
    ui::Compositor* compositor) {
  PerCompositorData* data = per_compositor_data_[compositor];
  if (!data)
    data = CreatePerCompositorData(compositor);

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kUIEnableSoftwareCompositing)) {
    context =
        CreateContextCommon(data->swap_client->AsWeakPtr(), data->surface_id);
  }
  if (!context) {
    if (ui::Compositor::WasInitializedWithThread()) {
      LOG(FATAL) << "Failed to create UI context, but can't use software "
                 " compositing with browser threaded compositing. Aborting.";
    }

    scoped_ptr<SoftwareBrowserCompositorOutputSurface> surface =
        SoftwareBrowserCompositorOutputSurface::Create(
            CreateSoftwareOutputDevice(compositor));
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
      new BrowserCompositorOutputSurface(
          context.PassAs<WebKit::WebGraphicsContext3D>(),
          per_compositor_data_[compositor]->surface_id,
          &output_surface_map_,
          base::MessageLoopProxy::current().get(),
          compositor->AsWeakPtr()));
  if (data->reflector.get()) {
    data->reflector->CreateSharedTexture();
    data->reflector->AttachToOutputSurface(surface.get());
  }

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
  if (per_compositor_data_.empty())
    gl_helper_.reset();
}

bool GpuProcessTransportFactory::DoesCreateTestContexts() { return false; }

ui::ContextFactory* GpuProcessTransportFactory::AsContextFactory() {
  return this;
}

gfx::GLSurfaceHandle GpuProcessTransportFactory::CreateSharedSurfaceHandle() {
  CreateSharedContextLazy();
  if (!shared_contexts_main_thread_ ||
      !shared_contexts_main_thread_->Context3d())
    return gfx::GLSurfaceHandle();
  gfx::GLSurfaceHandle handle = gfx::GLSurfaceHandle(
      gfx::kNullPluginWindow, gfx::TEXTURE_TRANSPORT);
  handle.parent_gpu_process_id =
      shared_contexts_main_thread_->Context3d()->GetGPUProcessID();
  handle.parent_client_id =
      shared_contexts_main_thread_->Context3d()->GetChannelID();
  return handle;
}

void GpuProcessTransportFactory::DestroySharedSurfaceHandle(
    gfx::GLSurfaceHandle surface) {}

scoped_refptr<ui::Texture> GpuProcessTransportFactory::CreateTransportClient(
    float device_scale_factor) {
  if (!shared_contexts_main_thread_.get())
    return NULL;
  scoped_refptr<ImageTransportClientTexture> image(
      new ImageTransportClientTexture(
          shared_contexts_main_thread_->Context3d(),
          device_scale_factor));
  return image;
}

scoped_refptr<ui::Texture> GpuProcessTransportFactory::CreateOwnedTexture(
    const gfx::Size& size,
    float device_scale_factor,
    unsigned int texture_id) {
  if (!shared_contexts_main_thread_.get())
    return NULL;
  scoped_refptr<OwnedTexture> image(new OwnedTexture(
      shared_contexts_main_thread_->Context3d(),
      size,
      device_scale_factor,
      texture_id));
  return image;
}

GLHelper* GpuProcessTransportFactory::GetGLHelper() {
  if (!gl_helper_) {
    CreateSharedContextLazy();
    WebGraphicsContext3DCommandBufferImpl* context_for_main_thread =
        shared_contexts_main_thread_->Context3d();
    gl_helper_.reset(new GLHelper(context_for_main_thread));
  }
  return gl_helper_.get();
}

uint32 GpuProcessTransportFactory::InsertSyncPoint() {
  if (!shared_contexts_main_thread_.get())
    return 0;
  return shared_contexts_main_thread_->Context3d()->insertSyncPoint();
}

void GpuProcessTransportFactory::WaitSyncPoint(uint32 sync_point) {
  if (!shared_contexts_main_thread_.get())
    return;
  shared_contexts_main_thread_->Context3d()->waitSyncPoint(sync_point);
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
GpuProcessTransportFactory::OffscreenContextProviderForMainThread() {
  if (!shared_contexts_main_thread_.get() ||
      shared_contexts_main_thread_->DestroyedOnMainThread()) {
    shared_contexts_main_thread_ = ContextProviderCommandBuffer::Create(
        base::Bind(&GpuProcessTransportFactory::
                       CreateOffscreenCommandBufferContext,
                   base::Unretained(this)));
    if (shared_contexts_main_thread_) {
      shared_contexts_main_thread_->SetLostContextCallback(base::Bind(
          &GpuProcessTransportFactory::
              OnLostMainThreadSharedContextInsideCallback,
          callback_factory_.GetWeakPtr()));

      if (!shared_contexts_main_thread_->BindToCurrentThread())
        shared_contexts_main_thread_ = NULL;
    }
  }
  return shared_contexts_main_thread_;
}

scoped_refptr<cc::ContextProvider>
GpuProcessTransportFactory::OffscreenContextProviderForCompositorThread() {
  if (!shared_contexts_compositor_thread_.get() ||
      shared_contexts_compositor_thread_->DestroyedOnMainThread()) {
    shared_contexts_compositor_thread_ = ContextProviderCommandBuffer::Create(
        base::Bind(&GpuProcessTransportFactory::
                       CreateOffscreenCommandBufferContext,
                   base::Unretained(this)));
  }
  return shared_contexts_compositor_thread_;
}

void GpuProcessTransportFactory::OnLostContext(ui::Compositor* compositor) {
  LOG(ERROR) << "Lost UI compositor context.";
  PerCompositorData* data = per_compositor_data_[compositor];
  DCHECK(data);

  // Prevent callbacks from other contexts in the same share group from
  // calling us again.
  data->swap_client.reset(new CompositorSwapClient(compositor, this));
  compositor->OnSwapBuffersAborted();
}

GpuProcessTransportFactory::PerCompositorData*
GpuProcessTransportFactory::CreatePerCompositorData(
    ui::Compositor* compositor) {
  DCHECK(!per_compositor_data_[compositor]);

  CreateSharedContextLazy();

  gfx::AcceleratedWidget widget = compositor->widget();
  GpuSurfaceTracker* tracker = GpuSurfaceTracker::Get();

  PerCompositorData* data = new PerCompositorData;
  data->surface_id = tracker->AddSurfaceForNativeWidget(widget);
  data->swap_client.reset(new CompositorSwapClient(compositor, this));
#if defined(OS_WIN)
  if (GpuDataManagerImpl::GetInstance()->IsUsingAcceleratedSurface())
    data->accelerated_surface.reset(new AcceleratedSurface(widget));
#endif
  tracker->SetSurfaceHandle(
      data->surface_id,
      gfx::GLSurfaceHandle(widget, gfx::NATIVE_DIRECT));

  per_compositor_data_[compositor] = data;

  return data;
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
GpuProcessTransportFactory::CreateContextCommon(
    const base::WeakPtr<WebGraphicsContext3DSwapBuffersClient>& swap_client,
    int surface_id) {
  if (!GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor())
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  WebKit::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  attrs.depth = false;
  attrs.stencil = false;
  attrs.antialias = false;
  attrs.noAutomaticFlushes = true;
  GpuChannelHostFactory* factory = BrowserGpuChannelHostFactory::instance();
  GURL url("chrome://gpu/GpuProcessTransportFactory::CreateContextCommon");
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(
          surface_id,
          url,
          factory,
          swap_client));
  if (!context->InitializeWithDefaultBufferSizes(
        attrs,
        false,
        CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE))
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  return context.Pass();
}

void GpuProcessTransportFactory::CreateSharedContextLazy() {
  scoped_refptr<cc::ContextProvider> provider =
      OffscreenContextProviderForMainThread();
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

  scoped_refptr<ContextProviderCommandBuffer> old_contexts_main_thread =
      shared_contexts_main_thread_;
  shared_contexts_main_thread_ = NULL;

  scoped_ptr<GLHelper> old_helper(gl_helper_.release());

  FOR_EACH_OBSERVER(ImageTransportFactoryObserver,
                    observer_list_,
                    OnLostResources());
}

}  // namespace content
