// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_impl_android.h"

#include <android/bitmap.h>
#include <android/native_window_jni.h>
#include <map>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/trees/layer_tree_host.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/browser/android/compositor_client.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace gfx {
class JavaBitmap;
}

namespace {

// Used for drawing directly to the screen. Bypasses resizing and swaps.
class DirectOutputSurface : public cc::OutputSurface {
 public:
  DirectOutputSurface(
      const scoped_refptr<cc::ContextProvider>& context_provider)
      : cc::OutputSurface(context_provider) {
    capabilities_.adjust_deadline_for_parent = false;
  }

  virtual void Reshape(gfx::Size size, float scale_factor) OVERRIDE {
    surface_size_ = size;
  }
  virtual void SwapBuffers(cc::CompositorFrame*) OVERRIDE {
    context_provider_->Context3d()->shallowFlushCHROMIUM();
  }
};

// Used to override capabilities_.adjust_deadline_for_parent to false
class OutputSurfaceWithoutParent : public cc::OutputSurface {
 public:
  OutputSurfaceWithoutParent(
      const scoped_refptr<
        content::ContextProviderCommandBuffer>& context_provider)
      : cc::OutputSurface(context_provider) {
    capabilities_.adjust_deadline_for_parent = false;
  }

  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE {
    content::WebGraphicsContext3DCommandBufferImpl* command_buffer_context =
        static_cast<content::WebGraphicsContext3DCommandBufferImpl*>(
            context_provider_->Context3d());
    content::CommandBufferProxyImpl* command_buffer_proxy =
        command_buffer_context->GetCommandBufferProxy();
    DCHECK(command_buffer_proxy);
    command_buffer_proxy->SetLatencyInfo(frame->metadata.latency_info);

    OutputSurface::SwapBuffers(frame);
  }
};

static bool g_initialized = false;
static base::Thread* g_impl_thread = NULL;
static bool g_use_direct_gl = false;

} // anonymous namespace

namespace content {

typedef std::map<int, base::android::ScopedJavaGlobalRef<jobject> >
    SurfaceMap;
static base::LazyInstance<SurfaceMap>
    g_surface_map = LAZY_INSTANCE_INITIALIZER;
static base::LazyInstance<base::Lock> g_surface_map_lock;

// static
Compositor* Compositor::Create(CompositorClient* client) {
  return client ? new CompositorImpl(client) : NULL;
}

// static
void Compositor::Initialize() {
  DCHECK(!CompositorImpl::IsInitialized());
  g_initialized = true;
}

// static
void Compositor::InitializeWithFlags(uint32 flags) {
  g_use_direct_gl = flags & DIRECT_CONTEXT_ON_DRAW_THREAD;
  if (flags & ENABLE_COMPOSITOR_THREAD) {
    TRACE_EVENT_INSTANT0("test_gpu", "ThreadedCompositingInitialization",
                         TRACE_EVENT_SCOPE_THREAD);
    g_impl_thread = new base::Thread("Browser Compositor");
    g_impl_thread->Start();
  }
  Compositor::Initialize();
}

// static
bool CompositorImpl::IsInitialized() {
  return g_initialized;
}

// static
bool CompositorImpl::IsThreadingEnabled() {
  return g_impl_thread;
}

// static
bool CompositorImpl::UsesDirectGL() {
  return g_use_direct_gl;
}

// static
jobject CompositorImpl::GetSurface(int surface_id) {
  base::AutoLock lock(g_surface_map_lock.Get());
  SurfaceMap* surfaces = g_surface_map.Pointer();
  SurfaceMap::iterator it = surfaces->find(surface_id);
  jobject jsurface = it == surfaces->end() ? NULL : it->second.obj();

  LOG_IF(WARNING, !jsurface) << "No surface for surface id " << surface_id;
  return jsurface;
}

CompositorImpl::CompositorImpl(CompositorClient* client)
    : root_layer_(cc::Layer::Create()),
      has_transparent_background_(false),
      window_(NULL),
      surface_id_(0),
      client_(client),
      weak_factory_(this) {
  DCHECK(client);
  ImageTransportFactoryAndroid::AddObserver(this);
}

CompositorImpl::~CompositorImpl() {
  ImageTransportFactoryAndroid::RemoveObserver(this);
  // Clean-up any surface references.
  SetSurface(NULL);
}

void CompositorImpl::SetNeedsRedraw() {
  if (host_)
    host_->SetNeedsRedraw();
}

void CompositorImpl::Composite() {
  if (host_)
    host_->Composite(base::TimeTicks::Now());
}

void CompositorImpl::SetRootLayer(scoped_refptr<cc::Layer> root_layer) {
  root_layer_->RemoveAllChildren();
  root_layer_->AddChild(root_layer);
}

void CompositorImpl::SetWindowSurface(ANativeWindow* window) {
  GpuSurfaceTracker* tracker = GpuSurfaceTracker::Get();

  if (window_) {
    tracker->RemoveSurface(surface_id_);
    ANativeWindow_release(window_);
    window_ = NULL;
    surface_id_ = 0;
    SetVisible(false);
  }

  if (window) {
    window_ = window;
    ANativeWindow_acquire(window);
    surface_id_ = tracker->AddSurfaceForNativeWidget(window);
    tracker->SetSurfaceHandle(
        surface_id_,
        gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NATIVE_DIRECT));
    SetVisible(true);
  }
}

void CompositorImpl::SetSurface(jobject surface) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_surface(env, surface);

  // First, cleanup any existing surface references.
  if (surface_id_) {
    DCHECK(g_surface_map.Get().find(surface_id_) !=
           g_surface_map.Get().end());
    base::AutoLock lock(g_surface_map_lock.Get());
    g_surface_map.Get().erase(surface_id_);
  }
  SetWindowSurface(NULL);

  // Now, set the new surface if we have one.
  ANativeWindow* window = NULL;
  if (surface)
    window = ANativeWindow_fromSurface(env, surface);
  if (window) {
    SetWindowSurface(window);
    ANativeWindow_release(window);
    {
      base::AutoLock lock(g_surface_map_lock.Get());
      g_surface_map.Get().insert(std::make_pair(surface_id_, j_surface));
    }
  }
}

void CompositorImpl::SetVisible(bool visible) {
  if (!visible) {
    ui_resource_map_.clear();
    host_.reset();
    client_->UIResourcesAreInvalid();
  } else if (!host_) {
    cc::LayerTreeSettings settings;
    settings.refresh_rate = 60.0;
    settings.impl_side_painting = false;
    settings.allow_antialiasing = false;
    settings.calculate_top_controls_position = false;
    settings.top_controls_height = 0.f;
    settings.use_memory_management = false;
    settings.highp_threshold_min = 2048;

    // Do not clear the framebuffer when rendering into external GL contexts
    // like Android View System's.
    if (UsesDirectGL())
      settings.should_clear_root_render_pass = false;

    scoped_refptr<base::SingleThreadTaskRunner> impl_thread_task_runner =
        g_impl_thread ? g_impl_thread->message_loop()->message_loop_proxy()
                      : NULL;

    host_ = cc::LayerTreeHost::Create(this, settings, impl_thread_task_runner);
    host_->SetRootLayer(root_layer_);

    host_->SetVisible(true);
    host_->SetLayerTreeHostClientReady();
    host_->SetViewportSize(size_);
    host_->set_has_transparent_background(has_transparent_background_);
    // Need to recreate the UI resources because a new LayerTreeHost has been
    // created.
    client_->DidLoseUIResources();
  }
}

void CompositorImpl::setDeviceScaleFactor(float factor) {
  if (host_)
    host_->SetDeviceScaleFactor(factor);
}

void CompositorImpl::SetWindowBounds(const gfx::Size& size) {
  if (size_ == size)
    return;

  size_ = size;
  if (host_)
    host_->SetViewportSize(size);
  root_layer_->SetBounds(size);
}

void CompositorImpl::SetHasTransparentBackground(bool flag) {
  has_transparent_background_ = flag;
  if (host_)
    host_->set_has_transparent_background(flag);
}

bool CompositorImpl::CompositeAndReadback(void *pixels, const gfx::Rect& rect) {
  if (host_)
    return host_->CompositeAndReadback(pixels, rect);
  else
    return false;
}

cc::UIResourceId CompositorImpl::GenerateUIResource(
    const cc::UIResourceBitmap& bitmap) {
  if (!host_)
    return 0;
  scoped_ptr<cc::ScopedUIResource> ui_resource =
      cc::ScopedUIResource::Create(host_.get(), bitmap);
  cc::UIResourceId id = ui_resource->id();
  ui_resource_map_.set(id, ui_resource.Pass());
  return id;
}

void CompositorImpl::DeleteUIResource(cc::UIResourceId resource_id) {
  UIResourceMap::iterator it = ui_resource_map_.find(resource_id);
  if (it != ui_resource_map_.end())
    ui_resource_map_.erase(it);
}

WebKit::WebGLId CompositorImpl::GenerateTexture(gfx::JavaBitmap& bitmap) {
  unsigned int texture_id = BuildBasicTexture();
  WebKit::WebGraphicsContext3D* context =
      ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
  if (texture_id == 0 || context->isContextLost() ||
      !context->makeContextCurrent())
    return 0;
  WebKit::WebGLId format = GetGLFormatForBitmap(bitmap);
  WebKit::WebGLId type = GetGLTypeForBitmap(bitmap);

  context->texImage2D(GL_TEXTURE_2D,
                      0,
                      format,
                      bitmap.size().width(),
                      bitmap.size().height(),
                      0,
                      format,
                      type,
                      bitmap.pixels());
  context->shallowFlushCHROMIUM();
  return texture_id;
}

WebKit::WebGLId CompositorImpl::GenerateCompressedTexture(gfx::Size& size,
                                                          int data_size,
                                                          void* data) {
  unsigned int texture_id = BuildBasicTexture();
  WebKit::WebGraphicsContext3D* context =
        ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
  if (texture_id == 0 || context->isContextLost() ||
      !context->makeContextCurrent())
    return 0;
  context->compressedTexImage2D(GL_TEXTURE_2D,
                                0,
                                GL_ETC1_RGB8_OES,
                                size.width(),
                                size.height(),
                                0,
                                data_size,
                                data);
  context->shallowFlushCHROMIUM();
  return texture_id;
}

void CompositorImpl::DeleteTexture(WebKit::WebGLId texture_id) {
  WebKit::WebGraphicsContext3D* context =
      ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
  if (context->isContextLost() || !context->makeContextCurrent())
    return;
  context->deleteTexture(texture_id);
  context->shallowFlushCHROMIUM();
}

bool CompositorImpl::CopyTextureToBitmap(WebKit::WebGLId texture_id,
                                         gfx::JavaBitmap& bitmap) {
  return CopyTextureToBitmap(texture_id, gfx::Rect(bitmap.size()), bitmap);
}

bool CompositorImpl::CopyTextureToBitmap(WebKit::WebGLId texture_id,
                                         const gfx::Rect& sub_rect,
                                         gfx::JavaBitmap& bitmap) {
  // The sub_rect should match the bitmap size.
  DCHECK(bitmap.size() == sub_rect.size());
  if (bitmap.size() != sub_rect.size() || texture_id == 0) return false;

  GLHelper* helper = ImageTransportFactoryAndroid::GetInstance()->GetGLHelper();
  helper->ReadbackTextureSync(texture_id,
                              sub_rect,
                              static_cast<unsigned char*> (bitmap.pixels()));
  return true;
}

static scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
CreateGpuProcessViewContext(
    const WebKit::WebGraphicsContext3D::Attributes attributes,
    int surface_id,
    base::WeakPtr<CompositorImpl> compositor_impl) {
  GpuChannelHostFactory* factory = BrowserGpuChannelHostFactory::instance();
  GURL url("chrome://gpu/Compositor::createContext3D");
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(surface_id,
                                                url,
                                                factory,
                                                compositor_impl));
  static const size_t kBytesPerPixel = 4;
  gfx::DeviceDisplayInfo display_info;
  size_t full_screen_texture_size_in_bytes =
      display_info.GetDisplayHeight() *
      display_info.GetDisplayWidth() *
      kBytesPerPixel;
  if (!context->Initialize(
          attributes,
          false,
          CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE,
          64 * 1024,  // command buffer size
          64 * 1024,  // start transfer buffer size
          64 * 1024,  // min transfer buffer size
          std::min(3 * full_screen_texture_size_in_bytes,
                   kDefaultMaxTransferBufferSize),
          2 * 1024 * 1024  // mapped memory limit
  )) {
    LOG(ERROR) << "Failed to create 3D context for compositor.";
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  }
  return context.Pass();
}

scoped_ptr<cc::OutputSurface> CompositorImpl::CreateOutputSurface(
    bool fallback) {
  WebKit::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  attrs.noAutomaticFlushes = true;

  if (g_use_direct_gl) {
    using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;
    scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d =
        WebGraphicsContext3DInProcessCommandBufferImpl::CreateViewContext(
            attrs, window_);
    scoped_refptr<webkit::gpu::ContextProviderInProcess> context_provider =
        webkit::gpu::ContextProviderInProcess::Create(context3d.Pass(),
                                                      "BrowserCompositor");

    scoped_ptr<cc::OutputSurface> output_surface;
    if (!window_)
      output_surface.reset(new DirectOutputSurface(context_provider));
    else
      output_surface.reset(new cc::OutputSurface(context_provider));
    return output_surface.Pass();
  }

  DCHECK(window_);
  DCHECK(surface_id_);

  scoped_refptr<ContextProviderCommandBuffer> context_provider =
      ContextProviderCommandBuffer::Create(CreateGpuProcessViewContext(
          attrs, surface_id_, weak_factory_.GetWeakPtr()), "BrowserCompositor");
  if (!context_provider.get()) {
    LOG(ERROR) << "Failed to create 3D context for compositor.";
    return scoped_ptr<cc::OutputSurface>();
  }

  return scoped_ptr<cc::OutputSurface>(
      new OutputSurfaceWithoutParent(context_provider));
}

void CompositorImpl::OnLostResources() {
  client_->DidLoseResources();
}

void CompositorImpl::DidCompleteSwapBuffers() {
  client_->OnSwapBuffersCompleted();
}

void CompositorImpl::ScheduleComposite() {
  client_->ScheduleComposite();
}

scoped_refptr<cc::ContextProvider>
CompositorImpl::OffscreenContextProviderForMainThread() {
  // There is no support for offscreen contexts, or compositor filters that
  // would require them in this compositor instance. If they are needed,
  // then implement a context provider that provides contexts from
  // ImageTransportSurfaceAndroid.
  return NULL;
}

scoped_refptr<cc::ContextProvider>
CompositorImpl::OffscreenContextProviderForCompositorThread() {
  // There is no support for offscreen contexts, or compositor filters that
  // would require them in this compositor instance. If they are needed,
  // then implement a context provider that provides contexts from
  // ImageTransportSurfaceAndroid.
  return NULL;
}

void CompositorImpl::OnViewContextSwapBuffersPosted() {
  TRACE_EVENT0("compositor", "CompositorImpl::OnViewContextSwapBuffersPosted");
  client_->OnSwapBuffersPosted();
}

void CompositorImpl::OnViewContextSwapBuffersComplete() {
  TRACE_EVENT0("compositor",
               "CompositorImpl::OnViewContextSwapBuffersComplete");
  client_->OnSwapBuffersCompleted();
}

void CompositorImpl::OnViewContextSwapBuffersAborted() {
  TRACE_EVENT0("compositor", "CompositorImpl::OnViewContextSwapBuffersAborted");
  client_->OnSwapBuffersCompleted();
}

WebKit::WebGLId CompositorImpl::BuildBasicTexture() {
  WebKit::WebGraphicsContext3D* context =
            ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
  if (context->isContextLost() || !context->makeContextCurrent())
    return 0;
  WebKit::WebGLId texture_id = context->createTexture();
  context->bindTexture(GL_TEXTURE_2D, texture_id);
  context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return texture_id;
}

WebKit::WGC3Denum CompositorImpl::GetGLFormatForBitmap(
    gfx::JavaBitmap& bitmap) {
  switch (bitmap.format()) {
    case ANDROID_BITMAP_FORMAT_A_8:
      return GL_ALPHA;
      break;
    case ANDROID_BITMAP_FORMAT_RGBA_4444:
      return GL_RGBA;
      break;
    case ANDROID_BITMAP_FORMAT_RGBA_8888:
      return GL_RGBA;
      break;
    case ANDROID_BITMAP_FORMAT_RGB_565:
    default:
      return GL_RGB;
  }
}

WebKit::WGC3Denum CompositorImpl::GetGLTypeForBitmap(gfx::JavaBitmap& bitmap) {
  switch (bitmap.format()) {
    case ANDROID_BITMAP_FORMAT_A_8:
      return GL_UNSIGNED_BYTE;
      break;
    case ANDROID_BITMAP_FORMAT_RGBA_4444:
      return GL_UNSIGNED_SHORT_4_4_4_4;
      break;
    case ANDROID_BITMAP_FORMAT_RGBA_8888:
      return GL_UNSIGNED_BYTE;
      break;
    case ANDROID_BITMAP_FORMAT_RGB_565:
    default:
      return GL_UNSIGNED_SHORT_5_6_5;
  }
}

} // namespace content
