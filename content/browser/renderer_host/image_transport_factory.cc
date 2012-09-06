// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/image_transport_factory.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/webkitplatformsupport_impl.h"
#include "content/public/common/content_switches.h"
#include "gpu/ipc/command_buffer_proxy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGraphicsContext3D.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_setup.h"
#include "ui/compositor/test_web_graphics_context_3d.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include "ui/surface/accelerated_surface_win.h"
#endif

using content::BrowserGpuChannelHostFactory;
using content::GLHelper;

namespace {

ImageTransportFactory* g_factory;

class DefaultTransportFactory
    : public ui::DefaultContextFactory,
      public ImageTransportFactory {
 public:
  DefaultTransportFactory() {
    ui::DefaultContextFactory::Initialize();
  }

  virtual ui::ContextFactory* AsContextFactory() OVERRIDE {
    return this;
  }

  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle() OVERRIDE {
    return gfx::GLSurfaceHandle();
  }

  virtual void DestroySharedSurfaceHandle(
      gfx::GLSurfaceHandle surface) OVERRIDE {
  }

  virtual scoped_refptr<ui::Texture> CreateTransportClient(
      const gfx::Size& size,
      uint64 transport_handle) OVERRIDE {
    return NULL;
  }

  virtual GLHelper* GetGLHelper() OVERRIDE {
    return NULL;
  }

  virtual uint32 InsertSyncPoint() OVERRIDE {
    return 0;
  }

  // We don't generate lost context events, so we don't need to keep track of
  // observers
  virtual void AddObserver(ImageTransportFactoryObserver* observer) OVERRIDE {
  }

  virtual void RemoveObserver(
      ImageTransportFactoryObserver* observer) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultTransportFactory);
};

class ImageTransportClientTexture : public ui::Texture {
 public:
  ImageTransportClientTexture(
      WebKit::WebGraphicsContext3D* host_context,
      const gfx::Size& size,
      uint64 surface_id)
          : ui::Texture(true, size),
            host_context_(host_context) {
    set_texture_id(surface_id);
  }

  virtual WebKit::WebGraphicsContext3D* HostContext3D() {
    return host_context_;
  }

 protected:
  virtual ~ImageTransportClientTexture() {}

 private:
  // A raw pointer. This |ImageTransportClientTexture| will be destroyed
  // before the |host_context_| via
  // |ImageTransportFactoryObserver::OnLostContext()| handlers.
  WebKit::WebGraphicsContext3D* host_context_;

  DISALLOW_COPY_AND_ASSIGN(ImageTransportClientTexture);
};

class GpuProcessTransportFactory;

class CompositorSwapClient
    : public base::SupportsWeakPtr<CompositorSwapClient>,
      public WebGraphicsContext3DSwapBuffersClient {
 public:
  CompositorSwapClient(ui::Compositor* compositor,
                       GpuProcessTransportFactory* factory)
      : compositor_(compositor),
        factory_(factory) {
  }

  ~CompositorSwapClient() {
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
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&CompositorSwapClient::OnLostContext, this->AsWeakPtr()));
  }

 private:
  void OnLostContext();
  ui::Compositor* compositor_;
  GpuProcessTransportFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorSwapClient);
};

class GpuProcessTransportFactory :
    public ui::ContextFactory,
    public ImageTransportFactory,
    public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  GpuProcessTransportFactory()
      : ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)) {
  }

  virtual ~GpuProcessTransportFactory() {
    DCHECK(per_compositor_data_.empty());
  }

  virtual WebKit::WebGraphicsContext3D* CreateContext(
      ui::Compositor* compositor) OVERRIDE {
    PerCompositorData* data = per_compositor_data_[compositor];
    if (!data)
      data = CreatePerCompositorData(compositor);
    return CreateContextCommon(data->swap_client->AsWeakPtr(),
                               data->surface_id);
  }

  virtual WebGraphicsContext3DCommandBufferImpl* CreateOffscreenContext()
      OVERRIDE {
    base::WeakPtr<WebGraphicsContext3DSwapBuffersClient> swap_client;
    return CreateContextCommon(swap_client, 0);
  }

  virtual void RemoveCompositor(ui::Compositor* compositor) OVERRIDE {
    PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
    if (it == per_compositor_data_.end())
      return;
    PerCompositorData* data = it->second;
    DCHECK(data);
    GpuSurfaceTracker::Get()->RemoveSurface(data->surface_id);
    delete data;
    per_compositor_data_.erase(it);
    if (per_compositor_data_.empty()) {
      gl_helper_.reset();
      shared_context_.reset();
      callback_factory_.InvalidateWeakPtrs();
    }
  }

  virtual ui::ContextFactory* AsContextFactory() OVERRIDE {
    return this;
  }

  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle() OVERRIDE {
    CreateSharedContextLazy();
    gfx::GLSurfaceHandle handle = gfx::GLSurfaceHandle(
        gfx::kNullPluginWindow, true);
    handle.parent_gpu_process_id = shared_context_->GetGPUProcessID();
    handle.parent_client_id = shared_context_->GetChannelID();
    handle.parent_context_id = shared_context_->GetContextID();
    handle.parent_texture_id[0] = shared_context_->createTexture();
    handle.parent_texture_id[1] = shared_context_->createTexture();
    handle.sync_point = shared_context_->insertSyncPoint();

    return handle;
  }

  virtual void DestroySharedSurfaceHandle(
      gfx::GLSurfaceHandle surface) OVERRIDE {
    if (!shared_context_.get())
      return;
    uint32 channel_id = shared_context_->GetChannelID();
    uint32 context_id = shared_context_->GetContextID();
    if (surface.parent_gpu_process_id != shared_context_->GetGPUProcessID() ||
        surface.parent_client_id != channel_id ||
        surface.parent_context_id != context_id)
      return;

    shared_context_->deleteTexture(surface.parent_texture_id[0]);
    shared_context_->deleteTexture(surface.parent_texture_id[1]);
    shared_context_->flush();
  }

  virtual scoped_refptr<ui::Texture> CreateTransportClient(
      const gfx::Size& size,
      uint64 transport_handle) {
    if (!shared_context_.get())
        return NULL;
    scoped_refptr<ImageTransportClientTexture> image(
        new ImageTransportClientTexture(shared_context_.get(),
                                        size, transport_handle));
    return image;
  }

  virtual GLHelper* GetGLHelper() {
    if (!gl_helper_.get()) {
      CreateSharedContextLazy();
      WebKit::WebGraphicsContext3D* context_for_thread =
          CreateOffscreenContext();
      if (!context_for_thread)
        return NULL;
      gl_helper_.reset(new GLHelper(shared_context_.get(),
                                    context_for_thread));
    }
    return gl_helper_.get();
  }

  virtual uint32 InsertSyncPoint() OVERRIDE {
    if (!shared_context_.get())
      return 0;
    return shared_context_->insertSyncPoint();
  }

  virtual void AddObserver(ImageTransportFactoryObserver* observer) {
    observer_list_.AddObserver(observer);
  }

  virtual void RemoveObserver(ImageTransportFactoryObserver* observer) {
    observer_list_.RemoveObserver(observer);
  }

  // WebGraphicsContextLostCallback implementation, called for the shared
  // context.
  virtual void onContextLost() {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&GpuProcessTransportFactory::OnLostSharedContext,
                   callback_factory_.GetWeakPtr()));
  }

  void OnLostContext(ui::Compositor* compositor) {
    LOG(ERROR) << "Lost UI compositor context.";
    PerCompositorData* data = per_compositor_data_[compositor];
    DCHECK(data);

    // Prevent callbacks from other contexts in the same share group from
    // calling us again.
    data->swap_client.reset(new CompositorSwapClient(compositor, this));
    compositor->OnSwapBuffersAborted();
  }

 private:
  struct PerCompositorData {
    int surface_id;
    scoped_ptr<CompositorSwapClient> swap_client;
#if defined(OS_WIN)
    scoped_ptr<AcceleratedSurface> accelerated_surface;
#endif
  };

  PerCompositorData* CreatePerCompositorData(ui::Compositor* compositor) {
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
    tracker->SetSurfaceHandle(
        data->surface_id,
        gfx::GLSurfaceHandle(widget, true));
#else
    tracker->SetSurfaceHandle(
        data->surface_id,
        gfx::GLSurfaceHandle(widget, false));
#endif

    per_compositor_data_[compositor] = data;

    return data;
  }

  WebGraphicsContext3DCommandBufferImpl* CreateContextCommon(
      const base::WeakPtr<WebGraphicsContext3DSwapBuffersClient>& swap_client,
      int surface_id) {
    WebKit::WebGraphicsContext3D::Attributes attrs;
    attrs.shareResources = true;
    attrs.depth = false;
    attrs.stencil = false;
    attrs.antialias = false;
    GpuChannelHostFactory* factory = BrowserGpuChannelHostFactory::instance();
    GURL url("chrome://gpu/GpuProcessTransportFactory::CreateContextCommon");
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
        new WebGraphicsContext3DCommandBufferImpl(
            surface_id,
            url,
            factory,
            swap_client));
    if (!context->Initialize(
        attrs,
        false,
        content::CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE))
      return NULL;
    return context.release();
  }

  void CreateSharedContextLazy() {
    if (shared_context_.get())
      return;

    shared_context_.reset(CreateOffscreenContext());
    if (!shared_context_.get()) {
      // If we can't recreate contexts, we won't be able to show the UI. Better
      // crash at this point.
      LOG(FATAL) << "Failed to initialize UI shared context.";
    }
    if (!shared_context_->makeContextCurrent()) {
      // If we can't recreate contexts, we won't be able to show the UI. Better
      // crash at this point.
      LOG(FATAL) << "Failed to make UI shared context current.";
    }
    shared_context_->setContextLostCallback(this);
  }

  void OnLostSharedContext() {
    // Keep old resources around while we call the observers, but ensure that
    // new resources are created if needed.
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> old_shared_context(
        shared_context_.release());
    scoped_ptr<GLHelper> old_helper(gl_helper_.release());

    FOR_EACH_OBSERVER(ImageTransportFactoryObserver,
        observer_list_,
        OnLostResources());
  }

  typedef std::map<ui::Compositor*, PerCompositorData*> PerCompositorDataMap;
  PerCompositorDataMap per_compositor_data_;
  scoped_ptr<GLHelper> gl_helper_;
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> shared_context_;
  ObserverList<ImageTransportFactoryObserver> observer_list_;
  base::WeakPtrFactory<GpuProcessTransportFactory> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessTransportFactory);
};

void CompositorSwapClient::OnLostContext() {
  factory_->OnLostContext(compositor_);
  // Note: previous line destroyed this. Don't access members from now on.
}

WebKit::WebGraphicsContext3D* CreateTestContext() {
  ui::TestWebGraphicsContext3D* test_context =
      new ui::TestWebGraphicsContext3D();
  test_context->Initialize();
  return test_context;
}

}  // anonymous namespace

// static
void ImageTransportFactory::Initialize() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTestCompositor)) {
    ui::SetupTestCompositor();
  }
  if (ui::IsTestCompositorEnabled()) {
    g_factory = new DefaultTransportFactory();
    content::WebKitPlatformSupportImpl::SetOffscreenContextFactoryForTest(
        CreateTestContext);
  } else {
    g_factory = new GpuProcessTransportFactory();
  }
  ui::ContextFactory::SetInstance(g_factory->AsContextFactory());
}

// static
void ImageTransportFactory::Terminate() {
  ui::ContextFactory::SetInstance(NULL);
  delete g_factory;
  g_factory = NULL;
}

// static
ImageTransportFactory* ImageTransportFactory::GetInstance() {
  return g_factory;
}
