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
#include "ui/base/android/window_android.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/frame_time.h"
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

} // anonymous namespace

namespace content {

typedef std::map<int, base::android::ScopedJavaGlobalRef<jobject> >
    SurfaceMap;
static base::LazyInstance<SurfaceMap>
    g_surface_map = LAZY_INSTANCE_INITIALIZER;
static base::LazyInstance<base::Lock> g_surface_map_lock;

// static
Compositor* Compositor::Create(CompositorClient* client,
                               gfx::NativeWindow root_window) {
  return client ? new CompositorImpl(client, root_window) : NULL;
}

// static
void Compositor::Initialize() {
  DCHECK(!CompositorImpl::IsInitialized());
  g_initialized = true;
}

// static
bool CompositorImpl::IsInitialized() {
  return g_initialized;
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

CompositorImpl::CompositorImpl(CompositorClient* client,
                               gfx::NativeWindow root_window)
    : root_layer_(cc::Layer::Create()),
      has_transparent_background_(false),
      window_(NULL),
      surface_id_(0),
      client_(client),
      root_window_(root_window) {
  DCHECK(client);
  DCHECK(root_window);
  ImageTransportFactoryAndroid::AddObserver(this);
  root_window->AttachCompositor();
}

CompositorImpl::~CompositorImpl() {
  root_window_->DetachCompositor();
  ImageTransportFactoryAndroid::RemoveObserver(this);
  // Clean-up any surface references.
  SetSurface(NULL);
}

void CompositorImpl::Composite() {
  if (host_)
    host_->Composite(gfx::FrameTime::Now());
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

    host_ = cc::LayerTreeHost::CreateSingleThreaded(this, this, NULL, settings);
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

blink::WebGLId CompositorImpl::GenerateTexture(gfx::JavaBitmap& bitmap) {
  unsigned int texture_id = BuildBasicTexture();
  blink::WebGraphicsContext3D* context =
      ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
  if (texture_id == 0 || context->isContextLost() ||
      !context->makeContextCurrent())
    return 0;
  blink::WebGLId format = GetGLFormatForBitmap(bitmap);
  blink::WebGLId type = GetGLTypeForBitmap(bitmap);

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

blink::WebGLId CompositorImpl::GenerateCompressedTexture(gfx::Size& size,
                                                          int data_size,
                                                          void* data) {
  unsigned int texture_id = BuildBasicTexture();
  blink::WebGraphicsContext3D* context =
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

void CompositorImpl::DeleteTexture(blink::WebGLId texture_id) {
  blink::WebGraphicsContext3D* context =
      ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
  if (context->isContextLost() || !context->makeContextCurrent())
    return;
  context->deleteTexture(texture_id);
  context->shallowFlushCHROMIUM();
}

bool CompositorImpl::CopyTextureToBitmap(blink::WebGLId texture_id,
                                         gfx::JavaBitmap& bitmap) {
  return CopyTextureToBitmap(texture_id, gfx::Rect(bitmap.size()), bitmap);
}

bool CompositorImpl::CopyTextureToBitmap(blink::WebGLId texture_id,
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
    const blink::WebGraphicsContext3D::Attributes attributes,
    int surface_id) {
  BrowserGpuChannelHostFactory* factory =
      BrowserGpuChannelHostFactory::instance();
  CauseForGpuLaunch cause =
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
  scoped_refptr<GpuChannelHost> gpu_channel_host(
      factory->EstablishGpuChannelSync(cause));
  if (!gpu_channel_host)
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();

  GURL url("chrome://gpu/Compositor::createContext3D");
  static const size_t kBytesPerPixel = 4;
  gfx::DeviceDisplayInfo display_info;
  size_t full_screen_texture_size_in_bytes =
      display_info.GetDisplayHeight() *
      display_info.GetDisplayWidth() *
      kBytesPerPixel;
  WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits limits;
  limits.command_buffer_size = 64 * 1024;
  limits.start_transfer_buffer_size = 64 * 1024;
  limits.min_transfer_buffer_size = 64 * 1024;
  limits.max_transfer_buffer_size = std::min(
      3 * full_screen_texture_size_in_bytes, kDefaultMaxTransferBufferSize);
  limits.mapped_memory_reclaim_limit = 2 * 1024 * 1024;
  return make_scoped_ptr(
      new WebGraphicsContext3DCommandBufferImpl(surface_id,
                                                url,
                                                gpu_channel_host.get(),
                                                attributes,
                                                false,
                                                limits));
}

scoped_ptr<cc::OutputSurface> CompositorImpl::CreateOutputSurface(
    bool fallback) {
  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  attrs.noAutomaticFlushes = true;

  DCHECK(window_);
  DCHECK(surface_id_);

  scoped_refptr<ContextProviderCommandBuffer> context_provider =
      ContextProviderCommandBuffer::Create(
          CreateGpuProcessViewContext(attrs, surface_id_), "BrowserCompositor");
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

scoped_refptr<cc::ContextProvider> CompositorImpl::OffscreenContextProvider() {
  // There is no support for offscreen contexts, or compositor filters that
  // would require them in this compositor instance. If they are needed,
  // then implement a context provider that provides contexts from
  // ImageTransportSurfaceAndroid.
  return NULL;
}

void CompositorImpl::DidCompleteSwapBuffers() {
  client_->OnSwapBuffersCompleted();
}

void CompositorImpl::ScheduleComposite() {
  client_->ScheduleComposite();
}

void CompositorImpl::ScheduleAnimation() {
  ScheduleComposite();
}

void CompositorImpl::DidPostSwapBuffers() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidPostSwapBuffers");
  client_->OnSwapBuffersPosted();
}

void CompositorImpl::DidAbortSwapBuffers() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidAbortSwapBuffers");
  client_->OnSwapBuffersCompleted();
}

blink::WebGLId CompositorImpl::BuildBasicTexture() {
  blink::WebGraphicsContext3D* context =
            ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
  if (context->isContextLost() || !context->makeContextCurrent())
    return 0;
  blink::WebGLId texture_id = context->createTexture();
  context->bindTexture(GL_TEXTURE_2D, texture_id);
  context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return texture_id;
}

blink::WGC3Denum CompositorImpl::GetGLFormatForBitmap(
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

blink::WGC3Denum CompositorImpl::GetGLTypeForBitmap(gfx::JavaBitmap& bitmap) {
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

void CompositorImpl::DidCommit() {
  root_window_->OnCompositingDidCommit();
}

} // namespace content
