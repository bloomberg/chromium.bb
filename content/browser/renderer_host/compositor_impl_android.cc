// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_impl_android.h"

#include <android/bitmap.h>
#include <android/native_window_jni.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "cc/font_atlas.h"
#include "cc/input_handler.h"
#include "cc/layer.h"
#include "cc/layer_tree_host.h"
#include "cc/output_surface.h"
#include "cc/thread_impl.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/common/content_switches.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "ui/gfx/android/java_bitmap.h"
#include "webkit/glue/webthread_impl.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"

namespace gfx {
class JavaBitmap;
}

namespace {

static bool g_initialized = false;
static webkit_glue::WebThreadImpl* g_impl_thread = NULL;
static bool g_use_direct_gl = false;

// Adapts a pure WebGraphicsContext3D into a cc::OutputSurface.
class WebGraphicsContextToOutputSurfaceAdapter : public cc::OutputSurface {
 public:
  explicit WebGraphicsContextToOutputSurfaceAdapter(
      WebKit::WebGraphicsContext3D* context)
      : context3d_(context),
        client_(0) {
  }

  virtual bool BindToClient(cc::OutputSurfaceClient* client) OVERRIDE {
    DCHECK(client);
    if (!context3d_->makeContextCurrent())
      return false;
    client_ = client;
    return true;
  }

  virtual const struct Capabilities& Capabilities() const OVERRIDE {
    return capabilities_;
  }

  virtual WebKit::WebGraphicsContext3D* Context3D() const OVERRIDE {
    return context3d_.get();
  }

  virtual cc::SoftwareOutputDevice* SoftwareDevice() const OVERRIDE {
    return NULL;
  }

  virtual void SendFrameToParentCompositor(
      cc::CompositorFrame*) OVERRIDE {
  }

 private:
  scoped_ptr<WebKit::WebGraphicsContext3D> context3d_;
  struct Capabilities capabilities_;
  cc::OutputSurfaceClient* client_;
};

} // anonymous namespace

namespace content {

// static
Compositor* Compositor::Create(Client* client) {
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
    TRACE_EVENT_INSTANT0("test_gpu", "ThreadedCompositingInitialization");
    g_impl_thread = new webkit_glue::WebThreadImpl("Browser Compositor");
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

CompositorImpl::CompositorImpl(Compositor::Client* client)
    : root_layer_(cc::Layer::create()),
      has_transparent_background_(false),
      window_(NULL),
      surface_id_(0),
      client_(client),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(client);
}

CompositorImpl::~CompositorImpl() {
}

void CompositorImpl::Composite() {
  if (host_.get())
    host_->composite();
}

void CompositorImpl::SetRootLayer(scoped_refptr<cc::Layer> root_layer) {
  root_layer_->removeAllChildren();
  root_layer_->addChild(root_layer);
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
        gfx::GLSurfaceHandle(gfx::kDummyPluginWindow, false));
    SetVisible(true);
  }
}

void CompositorImpl::SetVisible(bool visible) {
  if (!visible) {
    host_.reset();
  } else if (!host_.get()) {
    cc::LayerTreeSettings settings;
    settings.refreshRate = 60.0;
    settings.implSidePainting = false;
    settings.calculateTopControlsPosition = false;
    settings.topControlsHeightPx = 0;

    // Do not clear the framebuffer when rendering into external GL contexts
    // like Android View System's.
    if (UsesDirectGL())
      settings.shouldClearRootRenderPass = false;

    scoped_ptr<cc::Thread> impl_thread;
    if (g_impl_thread)
      impl_thread = cc::ThreadImpl::createForDifferentThread(
          g_impl_thread->message_loop()->message_loop_proxy());

    host_ = cc::LayerTreeHost::create(this, settings, impl_thread.Pass());
    host_->setRootLayer(root_layer_);

    host_->setVisible(true);
    host_->setSurfaceReady();
    host_->setViewportSize(size_, size_);
    host_->setHasTransparentBackground(has_transparent_background_);
  }
}

void CompositorImpl::SetWindowBounds(const gfx::Size& size) {
  if (size_ == size)
    return;

  size_ = size;
  if (host_)
    host_->setViewportSize(size, size);
  root_layer_->setBounds(size);
}

void CompositorImpl::SetHasTransparentBackground(bool flag) {
  has_transparent_background_ = flag;
  if (host_.get())
    host_->setHasTransparentBackground(flag);
}

bool CompositorImpl::CompositeAndReadback(void *pixels, const gfx::Rect& rect) {
  if (host_.get())
    return host_->compositeAndReadback(pixels, rect);
  else
    return false;
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
  DCHECK(context->getError() == GL_NO_ERROR);
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
  DCHECK(context->getError() == GL_NO_ERROR);
  return texture_id;
}

void CompositorImpl::DeleteTexture(WebKit::WebGLId texture_id) {
  WebKit::WebGraphicsContext3D* context =
      ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
  if (context->isContextLost() || !context->makeContextCurrent())
    return;
  context->deleteTexture(texture_id);
  context->shallowFlushCHROMIUM();
  DCHECK(context->getError() == GL_NO_ERROR);
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

void CompositorImpl::animate(double monotonicFrameBeginTime) {
}

void CompositorImpl::layout() {
}

void CompositorImpl::applyScrollAndScale(gfx::Vector2d scrollDelta,
                                         float pageScale) {
}

scoped_ptr<cc::OutputSurface> CompositorImpl::createOutputSurface() {
  if (g_use_direct_gl) {
    WebKit::WebGraphicsContext3D::Attributes attrs;
    attrs.shareResources = false;
    attrs.noAutomaticFlushes = true;
    scoped_ptr<webkit::gpu::WebGraphicsContext3DInProcessImpl> context(
        webkit::gpu::WebGraphicsContext3DInProcessImpl::CreateForWindow(
            attrs,
            window_,
            NULL));
    return scoped_ptr<cc::OutputSurface>(
        new WebGraphicsContextToOutputSurfaceAdapter(context.release()));
  } else {
    DCHECK(window_ && surface_id_);
    WebKit::WebGraphicsContext3D::Attributes attrs;
    attrs.shareResources = true;
    attrs.noAutomaticFlushes = true;
    GpuChannelHostFactory* factory = BrowserGpuChannelHostFactory::instance();
    GURL url("chrome://gpu/Compositor::createContext3D");
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
        new WebGraphicsContext3DCommandBufferImpl(surface_id_,
                                                  url,
                                                  factory,
                                                  weak_factory_.GetWeakPtr()));
    if (!context->Initialize(
        attrs,
        false,
        CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE)) {
      LOG(ERROR) << "Failed to create 3D context for compositor.";
      return scoped_ptr<cc::OutputSurface>();
    }
    return scoped_ptr<cc::OutputSurface>(
        new WebGraphicsContextToOutputSurfaceAdapter(context.release()));
  }
}

scoped_ptr<cc::InputHandler> CompositorImpl::createInputHandler() {
  return scoped_ptr<cc::InputHandler>();
}

void CompositorImpl::didRecreateOutputSurface(bool success) {
}

void CompositorImpl::didCommit() {
}

void CompositorImpl::didCommitAndDrawFrame() {
}

void CompositorImpl::didCompleteSwapBuffers() {
  client_->OnSwapBuffersCompleted();
}

void CompositorImpl::scheduleComposite() {
  client_->ScheduleComposite();
}

scoped_ptr<cc::FontAtlas> CompositorImpl::createFontAtlas() {
  return scoped_ptr<cc::FontAtlas>();
}

void CompositorImpl::OnViewContextSwapBuffersPosted() {
  TRACE_EVENT0("compositor", "CompositorImpl::OnViewContextSwapBuffersPosted");
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
  DCHECK(context->getError() == GL_NO_ERROR);
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
