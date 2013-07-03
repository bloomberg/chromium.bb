// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/image_transport_factory.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/common/content_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/ipc/command_buffer_proxy.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_setup.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test_web_graphics_context_3d.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include "content/browser/renderer_host/software_output_device_win.h"
#include "ui/surface/accelerated_surface_win.h"
#elif defined(USE_X11)
#include "content/browser/renderer_host/software_output_device_x11.h"
#endif

namespace content {
namespace {

ImageTransportFactory* g_factory;

// An ImageTransportFactory that disables transport.
class NoTransportFactory : public ImageTransportFactory {
 public:
  explicit NoTransportFactory(ui::ContextFactory* context_factory)
      : context_factory_(context_factory) {
  }

  virtual ui::ContextFactory* AsContextFactory() OVERRIDE {
    return context_factory_.get();
  }

  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle() OVERRIDE {
    return gfx::GLSurfaceHandle();
  }

  virtual void DestroySharedSurfaceHandle(
      gfx::GLSurfaceHandle surface) OVERRIDE {
  }

  virtual scoped_refptr<ui::Texture> CreateTransportClient(
      float device_scale_factor) OVERRIDE {
    return NULL;
  }

  virtual scoped_refptr<ui::Texture> CreateOwnedTexture(
      const gfx::Size& size,
      float device_scale_factor,
      unsigned int texture_id) OVERRIDE {
    return NULL;
  }

  virtual GLHelper* GetGLHelper() OVERRIDE {
    return NULL;
  }

  virtual uint32 InsertSyncPoint() OVERRIDE {
    return 0;
  }

  virtual void WaitSyncPoint(uint32 sync_point) OVERRIDE {
  }

  // We don't generate lost context events, so we don't need to keep track of
  // observers
  virtual void AddObserver(ImageTransportFactoryObserver* observer) OVERRIDE {
  }

  virtual void RemoveObserver(
      ImageTransportFactoryObserver* observer) OVERRIDE {
  }

 private:
  scoped_ptr<ui::ContextFactory> context_factory_;
  DISALLOW_COPY_AND_ASSIGN(NoTransportFactory);
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
  void OnLostContext();
  ui::Compositor* compositor_;
  GpuProcessTransportFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorSwapClient);
};

class BrowserCompositorOutputSurface;

// Directs vsync updates to the appropriate BrowserCompositorOutputSurface.
class BrowserCompositorOutputSurfaceProxy
    : public base::RefCountedThreadSafe<BrowserCompositorOutputSurfaceProxy> {
 public:
  BrowserCompositorOutputSurfaceProxy()
    : message_handler_set_(false) {
  }

  void AddSurface(BrowserCompositorOutputSurface* surface, int surface_id) {
    if (!message_handler_set_) {
      uint32 messages_to_filter[] = {GpuHostMsg_UpdateVSyncParameters::ID};
      BrowserGpuChannelHostFactory::instance()->SetHandlerForControlMessages(
          messages_to_filter,
          arraysize(messages_to_filter),
          base::Bind(&BrowserCompositorOutputSurfaceProxy::OnMessageReceived,
                     this),
          base::MessageLoop::current()->message_loop_proxy().get());
      message_handler_set_ = true;
    }
    surface_map_.AddWithID(surface, surface_id);
  }

  BrowserCompositorOutputSurface* GetOutputSurface(int surface_id) {
    return surface_map_.Lookup(surface_id);
  }

  void RemoveSurface(int surface_id) {
    surface_map_.Remove(surface_id);
  }

 private:
  void OnMessageReceived(const IPC::Message& message) {
    IPC_BEGIN_MESSAGE_MAP(BrowserCompositorOutputSurfaceProxy, message)
      IPC_MESSAGE_HANDLER(GpuHostMsg_UpdateVSyncParameters,
                          OnUpdateVSyncParameters);
    IPC_END_MESSAGE_MAP()
  }

  void OnUpdateVSyncParameters(int surface_id,
                               base::TimeTicks timebase,
                               base::TimeDelta interval);

  friend class
      base::RefCountedThreadSafe<BrowserCompositorOutputSurfaceProxy>;
  ~BrowserCompositorOutputSurfaceProxy() {}
  IDMap<BrowserCompositorOutputSurface> surface_map_;
  bool message_handler_set_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOutputSurfaceProxy);
};

// A reflector implementation that copies the framebuffer content
// to the texture, then draw it onto the mirroring compositor.
class ReflectorImpl : public ImageTransportFactoryObserver,
                      public base::SupportsWeakPtr<ReflectorImpl>,
                      public ui::Reflector {
 public:
  ReflectorImpl(ui::Compositor* mirrored_compositor,
                ui::Layer* mirroring_layer,
                BrowserCompositorOutputSurfaceProxy* output_surface_proxy,
                int surface_id)
      : texture_id_(0),
        texture_size_(mirrored_compositor->size()),
        output_surface_proxy_(output_surface_proxy),
        mirrored_compositor_(mirrored_compositor),
        mirroring_compositor_(mirroring_layer->GetCompositor()),
        mirroring_layer_(mirroring_layer),
        impl_message_loop_(ui::Compositor::GetCompositorMessageLoop()),
        main_message_loop_(base::MessageLoopProxy::current()),
        surface_id_(surface_id) {
    CreateSharedTexture();
    impl_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ReflectorImpl::InitOnImplThread,
                   this));
  }

  ui::Compositor* mirrored_compositor() {
    return mirrored_compositor_;
  }

  void InitOnImplThread();

  void Shutdown() {
    mirroring_compositor_ = NULL;
    mirroring_layer_ = NULL;
    shared_texture_ = NULL;
    impl_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ReflectorImpl::ShutdownOnImplThread,
                   this));
  }

  void ShutdownOnImplThread();

  // This must be called on ImplThread, or before the surface is passed to
  // ImplThread.
  void AttachToOutputSurface(BrowserCompositorOutputSurface* surface);

  // ui::Reflector overrides:
  virtual void OnMirroringCompositorResized() OVERRIDE {
    mirroring_compositor_->ScheduleFullRedraw();
  }

  // ImageTransportFactoryObsever overrides:
  virtual void OnLostResources() OVERRIDE {
    shared_texture_ = NULL;
    mirroring_layer_->SetExternalTexture(NULL);
  }

  // Called when the output surface's size has changed.
  // This must be called on ImplThread.
  void OnReshape(gfx::Size size) {
    if (texture_size_ == size)
      return;
    texture_size_ = size;
    DCHECK(texture_id_);
    gl_helper_->ResizeTexture(texture_id_, size);
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ReflectorImpl::UpdateTextureSizeOnMainThread,
                   this->AsWeakPtr(),
                   texture_size_));
  }

  // Called in |BrowserCompositorOutputSurface::SwapBuffers| to copy
  // the full screen image to the |texture_id_|. This must be called
  // on ImplThread.
  void OnSwapBuffers() {
    DCHECK(texture_id_);
    gl_helper_->CopyTextureFullImage(texture_id_, texture_size_);
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ReflectorImpl::FullRedrawOnMainThread,
                   this->AsWeakPtr(),
                   texture_size_));
  }

  // Called in |BrowserCompositorOutputSurface::PostSubBuffer| copy
  // the sub image given by |rect| to the texture.This must be called
  // on ImplThread.
  void OnPostSubBuffer(gfx::Rect rect) {
    DCHECK(texture_id_);
    gl_helper_->CopyTextureSubImage(texture_id_, rect);
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ReflectorImpl::UpdateSubBufferOnMainThread,
                   this->AsWeakPtr(),
                   texture_size_,
                   rect));
  }

  // Create a shared texture that will be used to copy the content of
  // mirrored compositor to the mirroring compositor.  This must be
  // called before the reflector is attached to OutputSurface to avoid
  // race with ImplThread accessing |texture_id_|.
  void CreateSharedTexture() {
    texture_id_ = ImageTransportFactory::GetInstance()->GetGLHelper()->
        CreateTexture();
    shared_texture_ = ImageTransportFactory::GetInstance()->
        CreateOwnedTexture(texture_size_, 1.0f, texture_id_);
    mirroring_layer_->SetExternalTexture(shared_texture_.get());
  }

 private:
  virtual ~ReflectorImpl() {
    // Make sure the reflector is deleted on main thread.
    DCHECK_EQ(main_message_loop_.get(),
              base::MessageLoopProxy::current().get());
  }

  void UpdateTextureSizeOnMainThread(gfx::Size size) {
    if (!mirroring_layer_)
      return;
    mirroring_layer_->SetBounds(gfx::Rect(size));
  }

  // Request full redraw on mirroring compositor.
  void FullRedrawOnMainThread(gfx::Size size) {
    if (!mirroring_compositor_)
      return;
    UpdateTextureSizeOnMainThread(size);
    mirroring_compositor_->ScheduleFullRedraw();
  }

  void UpdateSubBufferOnMainThread(gfx::Size size,
                                   gfx::Rect rect) {
    if (!mirroring_compositor_)
      return;
    UpdateTextureSizeOnMainThread(size);
    // Flip the coordinates to compositor's one.
    int y = size.height() - rect.y() - rect.height();
    gfx::Rect new_rect(rect.x(), y, rect.width(), rect.height());
    mirroring_layer_->SchedulePaint(new_rect);
  }

  // Request full redraw on mirrored compositor so that
  // the full content will be copied to mirroring compositor.
  void FullRedrawContentOnMainThread() {
    mirrored_compositor_->ScheduleFullRedraw();
  }

  static void DeleteOnMainThread(ReflectorImpl* reflector) {
    // reflector gets deleted when the function returns.
  }

  // These variables are initialized on MainThread before
  // the reflector is attached to the output surface. Once
  // attached, they must be accessed only on ImplThraed unless
  // the context is lost. When the context is lost, these
  // will be re-ininitiailzed when the new output-surface
  // is created on MainThread.
  int texture_id_;
  gfx::Size texture_size_;

  // Must be accessed only on ImplThread.
  scoped_refptr<BrowserCompositorOutputSurfaceProxy> output_surface_proxy_;
  scoped_ptr<GLHelper> gl_helper_;

  // Must be accessed only on MainThread.
  ui::Compositor* mirrored_compositor_;
  ui::Compositor* mirroring_compositor_;
  ui::Layer* mirroring_layer_;
  scoped_refptr<ui::Texture> shared_texture_;
  scoped_refptr<base::MessageLoopProxy> impl_message_loop_;
  scoped_refptr<base::MessageLoopProxy> main_message_loop_;
  int surface_id_;
};

// Adapts a WebGraphicsContext3DCommandBufferImpl into a
// cc::OutputSurface that also handles vsync parameter updates
// arriving from the GPU process.
class BrowserCompositorOutputSurface
    : public cc::OutputSurface,
      public base::NonThreadSafe {
 public:
  BrowserCompositorOutputSurface(
      scoped_ptr<WebKit::WebGraphicsContext3D> context,
      int surface_id,
      BrowserCompositorOutputSurfaceProxy* output_surface_proxy,
      base::MessageLoopProxy* compositor_message_loop,
      base::WeakPtr<ui::Compositor> compositor)
      : OutputSurface(context.Pass()),
        surface_id_(surface_id),
        output_surface_proxy_(output_surface_proxy),
        compositor_message_loop_(compositor_message_loop),
        compositor_(compositor) {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUIMaxFramesPending)) {
      std::string string_value = command_line->GetSwitchValueASCII(
          switches::kUIMaxFramesPending);
      int int_value;
      if (base::StringToInt(string_value, &int_value))
        capabilities_.max_frames_pending = int_value;
      else
        LOG(ERROR) << "Trouble parsing --" << switches::kUIMaxFramesPending;
    }
    capabilities_.adjust_deadline_for_parent = false;
    DetachFromThread();
  }

  virtual ~BrowserCompositorOutputSurface() {
    DCHECK(CalledOnValidThread());
    if (!HasClient())
      return;
    output_surface_proxy_->RemoveSurface(surface_id_);
  }

  virtual bool BindToClient(
      cc::OutputSurfaceClient* client) OVERRIDE {
    DCHECK(CalledOnValidThread());

    if (!OutputSurface::BindToClient(client))
      return false;

    output_surface_proxy_->AddSurface(this, surface_id_);
    return true;
  }

  void OnUpdateVSyncParameters(
      base::TimeTicks timebase, base::TimeDelta interval) {
    DCHECK(CalledOnValidThread());
    DCHECK(HasClient());
    OnVSyncParametersChanged(timebase, interval);
    compositor_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ui::Compositor::OnUpdateVSyncParameters,
                   compositor_, timebase, interval));
  }

  virtual void Reshape(gfx::Size size, float scale_factor) OVERRIDE {
    OutputSurface::Reshape(size, scale_factor);
    if (reflector_.get())
      reflector_->OnReshape(size);
  }

  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE {
    DCHECK(frame->gl_frame_data);

    WebGraphicsContext3DCommandBufferImpl* command_buffer =
        static_cast<WebGraphicsContext3DCommandBufferImpl*>(context3d());
    CommandBufferProxyImpl* command_buffer_proxy =
        command_buffer->GetCommandBufferProxy();
    DCHECK(command_buffer_proxy);
    context3d()->shallowFlushCHROMIUM();
    command_buffer_proxy->SetLatencyInfo(frame->metadata.latency_info);

    if (reflector_.get()) {
      if (frame->gl_frame_data->sub_buffer_rect ==
          gfx::Rect(frame->gl_frame_data->size))
        reflector_->OnSwapBuffers();
      else
        reflector_->OnPostSubBuffer(frame->gl_frame_data->sub_buffer_rect);
    }

    OutputSurface::SwapBuffers(frame);
  }

  void SetReflector(ReflectorImpl* reflector) {
    reflector_ = reflector;
  }

 private:
  int surface_id_;
  scoped_refptr<BrowserCompositorOutputSurfaceProxy> output_surface_proxy_;

  scoped_refptr<base::MessageLoopProxy> compositor_message_loop_;
  base::WeakPtr<ui::Compositor> compositor_;
  scoped_refptr<ReflectorImpl> reflector_;
};

void BrowserCompositorOutputSurfaceProxy::OnUpdateVSyncParameters(
    int surface_id, base::TimeTicks timebase, base::TimeDelta interval) {
  BrowserCompositorOutputSurface* surface = surface_map_.Lookup(surface_id);
  if (surface)
    surface->OnUpdateVSyncParameters(timebase, interval);
}

void ReflectorImpl::AttachToOutputSurface(
    BrowserCompositorOutputSurface* output_surface) {
  gl_helper_.reset(new GLHelper(output_surface->context3d()));
  output_surface->SetReflector(this);
}

void ReflectorImpl::InitOnImplThread() {
  AttachToOutputSurface(output_surface_proxy_->GetOutputSurface(surface_id_));
  gl_helper_->CopyTextureFullImage(texture_id_, texture_size_);
  // The shared texture doesn't have the data, so invokes full redraw
  // now.
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::FullRedrawContentOnMainThread,
                 scoped_refptr<ReflectorImpl>(this)));
}

void ReflectorImpl::ShutdownOnImplThread() {
  BrowserCompositorOutputSurface* output_surface =
      output_surface_proxy_->GetOutputSurface(surface_id_);
  output_surface->SetReflector(NULL);
  gl_helper_.reset();
  // The instance must be deleted on main thread.
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::DeleteOnMainThread,
                 scoped_refptr<ReflectorImpl>(this)));
}

class GpuProcessTransportFactory
    : public ui::ContextFactory,
      public ImageTransportFactory {
 public:
  GpuProcessTransportFactory()
      : callback_factory_(this) {
    output_surface_proxy_ = new BrowserCompositorOutputSurfaceProxy();
  }

  virtual ~GpuProcessTransportFactory() {
    DCHECK(per_compositor_data_.empty());
  }

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
  CreateOffscreenCommandBufferContext() {
    base::WeakPtr<WebGraphicsContext3DSwapBuffersClient> swap_client;
    return CreateContextCommon(swap_client, 0);
  }

  virtual scoped_ptr<WebKit::WebGraphicsContext3D> CreateOffscreenContext()
      OVERRIDE {
    return CreateOffscreenCommandBufferContext()
        .PassAs<WebKit::WebGraphicsContext3D>();
  }

  virtual cc::OutputSurface* CreateOutputSurface(
      ui::Compositor* compositor) OVERRIDE {
    PerCompositorData* data = per_compositor_data_[compositor];
    if (!data)
      data = CreatePerCompositorData(compositor);
    BrowserCompositorOutputSurface* surface =
        new BrowserCompositorOutputSurface(
            CreateContextCommon(data->swap_client->AsWeakPtr(),
                                data->surface_id)
                .PassAs<WebKit::WebGraphicsContext3D>(),
            per_compositor_data_[compositor]->surface_id,
            output_surface_proxy_.get(),
            base::MessageLoopProxy::current().get(),
            compositor->AsWeakPtr());
    if (data->reflector.get()) {
      data->reflector->CreateSharedTexture();
      data->reflector->AttachToOutputSurface(surface);
    }
    return surface;
  }

  virtual scoped_refptr<ui::Reflector> CreateReflector(
      ui::Compositor* source,
      ui::Layer* target) OVERRIDE {
    PerCompositorData* data = per_compositor_data_[source];
    DCHECK(data);

    if (data->reflector.get())
      RemoveObserver(data->reflector.get());

    data->reflector = new ReflectorImpl(
        source, target, output_surface_proxy_.get(), data->surface_id);
    AddObserver(data->reflector.get());
    return data->reflector;
  }

  virtual void RemoveReflector(
      scoped_refptr<ui::Reflector> reflector) OVERRIDE {
    ReflectorImpl* reflector_impl =
        static_cast<ReflectorImpl*>(reflector.get());
    PerCompositorData* data =
        per_compositor_data_[reflector_impl->mirrored_compositor()];
    DCHECK(data);
    RemoveObserver(reflector_impl);
    data->reflector->Shutdown();
    data->reflector = NULL;
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
      callback_factory_.InvalidateWeakPtrs();
    }
  }

  virtual ui::ContextFactory* AsContextFactory() OVERRIDE {
    return this;
  }

  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle() OVERRIDE {
    CreateSharedContextLazy();
    gfx::GLSurfaceHandle handle = gfx::GLSurfaceHandle(
        gfx::kNullPluginWindow, gfx::TEXTURE_TRANSPORT);
    handle.parent_gpu_process_id =
        shared_contexts_main_thread_->Context3d()->GetGPUProcessID();
    handle.parent_client_id =
        shared_contexts_main_thread_->Context3d()->GetChannelID();
    return handle;
  }

  virtual void DestroySharedSurfaceHandle(
      gfx::GLSurfaceHandle surface) OVERRIDE {
  }

  virtual scoped_refptr<ui::Texture> CreateTransportClient(
      float device_scale_factor) OVERRIDE {
    if (!shared_contexts_main_thread_.get())
      return NULL;
    scoped_refptr<ImageTransportClientTexture> image(
        new ImageTransportClientTexture(
            shared_contexts_main_thread_->Context3d(),
            device_scale_factor));
    return image;
  }

  virtual scoped_refptr<ui::Texture> CreateOwnedTexture(
      const gfx::Size& size,
      float device_scale_factor,
      unsigned int texture_id) OVERRIDE {
    if (!shared_contexts_main_thread_.get())
      return NULL;
    scoped_refptr<OwnedTexture> image(new OwnedTexture(
        shared_contexts_main_thread_->Context3d(),
        size,
        device_scale_factor,
        texture_id));
    return image;
  }

  virtual GLHelper* GetGLHelper() OVERRIDE {
    if (!gl_helper_) {
      CreateSharedContextLazy();
      WebGraphicsContext3DCommandBufferImpl* context_for_main_thread =
          shared_contexts_main_thread_->Context3d();
      gl_helper_.reset(new GLHelper(context_for_main_thread));
    }
    return gl_helper_.get();
  }

  virtual uint32 InsertSyncPoint() OVERRIDE {
    if (!shared_contexts_main_thread_.get())
      return 0;
    return shared_contexts_main_thread_->Context3d()->insertSyncPoint();
  }

  virtual void WaitSyncPoint(uint32 sync_point) OVERRIDE {
    if (!shared_contexts_main_thread_.get())
      return;
    shared_contexts_main_thread_->Context3d()->waitSyncPoint(sync_point);
  }

  virtual void AddObserver(ImageTransportFactoryObserver* observer) OVERRIDE {
    observer_list_.AddObserver(observer);
  }

  virtual void RemoveObserver(
      ImageTransportFactoryObserver* observer) OVERRIDE {
    observer_list_.RemoveObserver(observer);
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
    scoped_refptr<ReflectorImpl> reflector;
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
#endif
    tracker->SetSurfaceHandle(
        data->surface_id,
        gfx::GLSurfaceHandle(widget, gfx::NATIVE_DIRECT));

    per_compositor_data_[compositor] = data;

    return data;
  }

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateContextCommon(
      const base::WeakPtr<WebGraphicsContext3DSwapBuffersClient>& swap_client,
      int surface_id) {
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

  // Crash given that we are unable to show any UI whatsoever. On Windows we
  // also trigger code in breakpad causes a system process to show message box.
  // In all cases a crash dump is generated.
  void FatalGPUError(const char* message) {
#if defined(OS_WIN)
    // 0xC01E0200 corresponds to STATUS_GRAPHICS_GPU_EXCEPTION_ON_DEVICE.
    ::RaiseException(0xC01E0200, EXCEPTION_NONCONTINUABLE, 0, NULL);
#else
    LOG(FATAL) << message;
#endif
  }

  class MainThreadContextProvider : public ContextProviderCommandBuffer {
   public:
    static scoped_refptr<MainThreadContextProvider> Create(
        GpuProcessTransportFactory* factory) {
      scoped_refptr<MainThreadContextProvider> provider =
          new MainThreadContextProvider(factory);
      if (!provider->InitializeOnMainThread())
        return NULL;
      return provider;
    }

   protected:
    explicit MainThreadContextProvider(GpuProcessTransportFactory* factory)
        : factory_(factory) {}
    virtual ~MainThreadContextProvider() {}

    virtual scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
        CreateOffscreenContext3d() OVERRIDE {
      return factory_->CreateOffscreenCommandBufferContext();
    }

    virtual void OnLostContext() OVERRIDE {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&GpuProcessTransportFactory::OnLostMainThreadSharedContext,
                     factory_->callback_factory_.GetWeakPtr()));
    }

   private:
    GpuProcessTransportFactory* factory_;
  };

  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE {
    if (!shared_contexts_main_thread_.get() ||
        shared_contexts_main_thread_->DestroyedOnMainThread()) {
      shared_contexts_main_thread_ = MainThreadContextProvider::Create(this);
      if (shared_contexts_main_thread_.get() &&
          !shared_contexts_main_thread_->BindToCurrentThread())
        shared_contexts_main_thread_ = NULL;
    }
    return shared_contexts_main_thread_;
  }

  class CompositorThreadContextProvider : public ContextProviderCommandBuffer {
   public:
    static scoped_refptr<CompositorThreadContextProvider> Create(
        GpuProcessTransportFactory* factory) {
      scoped_refptr<CompositorThreadContextProvider> provider =
          new CompositorThreadContextProvider(factory);
      if (!provider->InitializeOnMainThread())
        return NULL;
      return provider;
    }

   protected:
    explicit CompositorThreadContextProvider(
        GpuProcessTransportFactory* factory) : factory_(factory) {}
    virtual ~CompositorThreadContextProvider() {}

    virtual scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
        CreateOffscreenContext3d() OVERRIDE {
      return factory_->CreateOffscreenCommandBufferContext();
    }

   private:
    GpuProcessTransportFactory* factory_;
  };

  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE {
    if (!shared_contexts_compositor_thread_.get() ||
        shared_contexts_compositor_thread_->DestroyedOnMainThread()) {
      shared_contexts_compositor_thread_ =
          CompositorThreadContextProvider::Create(this);
    }
    return shared_contexts_compositor_thread_;
  }

  void CreateSharedContextLazy() {
    scoped_refptr<cc::ContextProvider> provider =
        OffscreenContextProviderForMainThread();
    if (!provider.get()) {
      // If we can't recreate contexts, we won't be able to show the UI.
      // Better crash at this point.
      FatalGPUError("Failed to initialize UI shared context.");
    }
  }

  void OnLostMainThreadSharedContext() {
    LOG(ERROR) << "Lost UI shared context.";
    // Keep old resources around while we call the observers, but ensure that
    // new resources are created if needed.

    scoped_refptr<MainThreadContextProvider> old_contexts_main_thread =
        shared_contexts_main_thread_;
    shared_contexts_main_thread_ = NULL;

    scoped_ptr<GLHelper> old_helper(gl_helper_.release());

    FOR_EACH_OBSERVER(ImageTransportFactoryObserver,
        observer_list_,
        OnLostResources());
  }

  typedef std::map<ui::Compositor*, PerCompositorData*> PerCompositorDataMap;
  PerCompositorDataMap per_compositor_data_;
  scoped_refptr<MainThreadContextProvider> shared_contexts_main_thread_;
  scoped_refptr<CompositorThreadContextProvider>
      shared_contexts_compositor_thread_;
  scoped_ptr<GLHelper> gl_helper_;
  ObserverList<ImageTransportFactoryObserver> observer_list_;
  base::WeakPtrFactory<GpuProcessTransportFactory> callback_factory_;
  scoped_refptr<BrowserCompositorOutputSurfaceProxy> output_surface_proxy_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessTransportFactory);
};

void CompositorSwapClient::OnLostContext() {
  factory_->OnLostContext(compositor_);
  // Note: previous line destroyed this. Don't access members from now on.
}

class SoftwareOutputSurface : public cc::OutputSurface {
 public:
  explicit SoftwareOutputSurface(
      scoped_ptr<cc::SoftwareOutputDevice> software_device)
      : cc::OutputSurface(software_device.Pass()) {}

  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE {
    ui::LatencyInfo latency_info = frame->metadata.latency_info;
    latency_info.swap_timestamp = base::TimeTicks::HighResNow();
    RenderWidgetHostImpl::CompositorFrameDrawn(latency_info);
    cc::OutputSurface::SwapBuffers(frame);
  }
};

class SoftwareContextFactory : public ui::ContextFactory {
 public:
  SoftwareContextFactory() {}
  virtual ~SoftwareContextFactory() {}
  virtual scoped_ptr<WebKit::WebGraphicsContext3D> CreateOffscreenContext()
      OVERRIDE {
    return scoped_ptr<WebKit::WebGraphicsContext3D>();
  }
  virtual cc::OutputSurface* CreateOutputSurface(
      ui::Compositor* compositor) OVERRIDE {
#if defined(OS_WIN)
    scoped_ptr<SoftwareOutputDeviceWin> software_device(
        new SoftwareOutputDeviceWin(compositor));
    return new SoftwareOutputSurface(
        software_device.PassAs<cc::SoftwareOutputDevice>());
#elif defined(USE_X11)
    scoped_ptr<SoftwareOutputDeviceX11> software_device(
        new SoftwareOutputDeviceX11(compositor));
    return new SoftwareOutputSurface(
        software_device.PassAs<cc::SoftwareOutputDevice>());
#else
    NOTIMPLEMENTED();
    return NULL;
#endif
  }

  virtual scoped_refptr<ui::Reflector> CreateReflector(
      ui::Compositor* source,
      ui::Layer* target) OVERRIDE {
    return NULL;
  }
  virtual void RemoveReflector(
      scoped_refptr<ui::Reflector> reflector) OVERRIDE {}

  virtual void RemoveCompositor(ui::Compositor* compositor) OVERRIDE {}

  virtual scoped_refptr<cc::ContextProvider>
  OffscreenContextProviderForMainThread() OVERRIDE {
    return NULL;
  }

  virtual scoped_refptr<cc::ContextProvider>
  OffscreenContextProviderForCompositorThread() OVERRIDE {
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SoftwareContextFactory);
};

}  // anonymous namespace

// static
void ImageTransportFactory::Initialize() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTestCompositor)) {
    ui::SetupTestCompositor();
  }
  if (ui::IsTestCompositorEnabled()) {
    g_factory = new NoTransportFactory(new ui::TestContextFactory);
  } else if (command_line->HasSwitch(switches::kUIEnableSoftwareCompositing)) {
    g_factory = new NoTransportFactory(new SoftwareContextFactory);
  } else {
    g_factory = new GpuProcessTransportFactory;
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

}  // namespace content
