// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/in_process_view_renderer.h"

#include <android/bitmap.h>

#include "android_webview/browser/aw_gl_surface.h"
#include "android_webview/browser/scoped_app_gl_state_restore.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/public/browser/draw_gl.h"
#include "android_webview/public/browser/draw_sw.h"
#include "base/android/jni_android.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/utils/SkCanvasStateUtils.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/vector2d_conversions.h"
#include "ui/gfx/vector2d_f.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

const void* kUserDataKey = &kUserDataKey;

class UserData : public content::WebContents::Data {
 public:
  UserData(InProcessViewRenderer* ptr) : instance_(ptr) {}
  virtual ~UserData() {
    instance_->WebContentsGone();
  }

  static InProcessViewRenderer* GetInstance(content::WebContents* contents) {
    if (!contents)
      return NULL;
    UserData* data = reinterpret_cast<UserData*>(
        contents->GetUserData(kUserDataKey));
    return data ? data->instance_ : NULL;
  }

 private:
  InProcessViewRenderer* instance_;
};

bool RasterizeIntoBitmap(JNIEnv* env,
                         const JavaRef<jobject>& jbitmap,
                         int scroll_x,
                         int scroll_y,
                         const InProcessViewRenderer::RenderMethod& renderer) {
  DCHECK(jbitmap.obj());

  AndroidBitmapInfo bitmap_info;
  if (AndroidBitmap_getInfo(env, jbitmap.obj(), &bitmap_info) < 0) {
    LOG(ERROR) << "Error getting java bitmap info.";
    return false;
  }

  void* pixels = NULL;
  if (AndroidBitmap_lockPixels(env, jbitmap.obj(), &pixels) < 0) {
    LOG(ERROR) << "Error locking java bitmap pixels.";
    return false;
  }

  bool succeeded;
  {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     bitmap_info.width,
                     bitmap_info.height,
                     bitmap_info.stride);
    bitmap.setPixels(pixels);

    SkBitmapDevice device(bitmap);
    SkCanvas canvas(&device);
    canvas.translate(-scroll_x, -scroll_y);
    succeeded = renderer.Run(&canvas);
  }

  if (AndroidBitmap_unlockPixels(env, jbitmap.obj()) < 0) {
    LOG(ERROR) << "Error unlocking java bitmap pixels.";
    return false;
  }

  return succeeded;
}

class ScopedPixelAccess {
 public:
  ScopedPixelAccess(JNIEnv* env, jobject java_canvas) {
    AwDrawSWFunctionTable* sw_functions =
        BrowserViewRenderer::GetAwDrawSWFunctionTable();
    pixels_ = sw_functions ?
      sw_functions->access_pixels(env, java_canvas) : NULL;
  }
  ~ScopedPixelAccess() {
    if (pixels_)
      BrowserViewRenderer::GetAwDrawSWFunctionTable()->release_pixels(pixels_);
  }
  AwPixelInfo* pixels() { return pixels_; }

 private:
  AwPixelInfo* pixels_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedPixelAccess);
};

bool HardwareEnabled() {
  static bool g_hw_enabled = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebViewGLMode);
  return g_hw_enabled;
}

// Provides software rendering functions from the Android glue layer.
// Allows preventing extra copies of data when rendering.
AwDrawSWFunctionTable* g_sw_draw_functions = NULL;

const int64 kFallbackTickTimeoutInMilliseconds = 20;

// Used to calculate memory and resource allocation. Determined experimentally.
size_t g_memory_multiplier = 10;
size_t g_num_gralloc_limit = 150;
const size_t kBytesPerPixel = 4;
const size_t kMemoryAllocationStep = 5 * 1024 * 1024;

base::LazyInstance<GLViewRendererManager>::Leaky g_view_renderer_manager =
    LAZY_INSTANCE_INITIALIZER;

class ScopedAllowGL {
 public:
  ScopedAllowGL();
  ~ScopedAllowGL();

  static bool IsAllowed() {
    return g_view_renderer_manager.Get().OnRenderThread() && allow_gl;
  }

 private:
  static bool allow_gl;

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowGL);
};

ScopedAllowGL::ScopedAllowGL() {
  DCHECK(g_view_renderer_manager.Get().OnRenderThread());
  DCHECK(!allow_gl);
  allow_gl = true;
}

ScopedAllowGL::~ScopedAllowGL() {
  allow_gl = false;
}

bool ScopedAllowGL::allow_gl = false;

void RequestProcessGLOnUIThread() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(&RequestProcessGLOnUIThread));
    return;
  }

  InProcessViewRenderer* renderer = static_cast<InProcessViewRenderer*>(
      g_view_renderer_manager.Get().GetMostRecentlyDrawn());
  if (!renderer || !renderer->RequestProcessGL()) {
    LOG(ERROR) << "Failed to request GL process. Deadlock likely: "
               << !!renderer;
  }
}

class DeferredGpuCommandService
    : public gpu::InProcessCommandBuffer::Service,
      public base::RefCountedThreadSafe<DeferredGpuCommandService> {
 public:
  DeferredGpuCommandService();

  virtual void ScheduleTask(const base::Closure& task) OVERRIDE;
  virtual void ScheduleIdleWork(const base::Closure& task) OVERRIDE;
  virtual bool UseVirtualizedGLContexts() OVERRIDE;

  void RunTasks();

  virtual void AddRef() const OVERRIDE {
    base::RefCountedThreadSafe<DeferredGpuCommandService>::AddRef();
  }
  virtual void Release() const OVERRIDE {
    base::RefCountedThreadSafe<DeferredGpuCommandService>::Release();
  }

 protected:
  virtual ~DeferredGpuCommandService();
  friend class base::RefCountedThreadSafe<DeferredGpuCommandService>;

 private:
  base::Lock tasks_lock_;
  std::queue<base::Closure> tasks_;
  DISALLOW_COPY_AND_ASSIGN(DeferredGpuCommandService);
};

DeferredGpuCommandService::DeferredGpuCommandService() {}

DeferredGpuCommandService::~DeferredGpuCommandService() {
  base::AutoLock lock(tasks_lock_);
  DCHECK(tasks_.empty());
}

// Called from different threads!
void DeferredGpuCommandService::ScheduleTask(const base::Closure& task) {
  {
    base::AutoLock lock(tasks_lock_);
    tasks_.push(task);
  }
  if (ScopedAllowGL::IsAllowed()) {
    RunTasks();
  } else {
    RequestProcessGLOnUIThread();
  }
}

void DeferredGpuCommandService::ScheduleIdleWork(
    const base::Closure& callback) {
  // TODO(sievers): Should this do anything?
}

bool DeferredGpuCommandService::UseVirtualizedGLContexts() { return true; }

void DeferredGpuCommandService::RunTasks() {
  bool has_more_tasks;
  {
    base::AutoLock lock(tasks_lock_);
    has_more_tasks = tasks_.size() > 0;
  }

  while (has_more_tasks) {
    base::Closure task;
    {
      base::AutoLock lock(tasks_lock_);
      task = tasks_.front();
      tasks_.pop();
    }
    task.Run();
    {
      base::AutoLock lock(tasks_lock_);
      has_more_tasks = tasks_.size() > 0;
    }

  }
}

base::LazyInstance<scoped_refptr<DeferredGpuCommandService> > g_service =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void BrowserViewRenderer::SetAwDrawSWFunctionTable(
    AwDrawSWFunctionTable* table) {
  g_sw_draw_functions = table;
}

// static
AwDrawSWFunctionTable* BrowserViewRenderer::GetAwDrawSWFunctionTable() {
  return g_sw_draw_functions;
}

InProcessViewRenderer::InProcessViewRenderer(
    BrowserViewRenderer::Client* client,
    JavaHelper* java_helper,
    content::WebContents* web_contents)
    : client_(client),
      java_helper_(java_helper),
      web_contents_(web_contents),
      compositor_(NULL),
      is_paused_(false),
      view_visible_(false),
      window_visible_(false),
      attached_to_window_(false),
      dip_scale_(0.0),
      page_scale_factor_(1.0),
      on_new_picture_enable_(false),
      clear_view_(false),
      compositor_needs_continuous_invalidate_(false),
      block_invalidates_(false),
      width_(0),
      height_(0),
      hardware_initialized_(false),
      hardware_failed_(false),
      last_egl_context_(NULL),
      manager_key_(g_view_renderer_manager.Get().NullKey()) {
  CHECK(web_contents_);
  web_contents_->SetUserData(kUserDataKey, new UserData(this));
  content::SynchronousCompositor::SetClientForWebContents(web_contents_, this);

  // Currently the logic in this class relies on |compositor_| remaining NULL
  // until the DidInitializeCompositor() call, hence it is not set here.
}

InProcessViewRenderer::~InProcessViewRenderer() {
  CHECK(web_contents_);
  content::SynchronousCompositor::SetClientForWebContents(web_contents_, NULL);
  web_contents_->SetUserData(kUserDataKey, NULL);
  NoLongerExpectsDrawGL();
  DCHECK(web_contents_ == NULL);  // WebContentsGone should have been called.
}

void InProcessViewRenderer::NoLongerExpectsDrawGL() {
  GLViewRendererManager& mru = g_view_renderer_manager.Get();
  if (manager_key_ != mru.NullKey()) {
    mru.NoLongerExpectsDrawGL(manager_key_);
    manager_key_ = mru.NullKey();
  }
}

// static
InProcessViewRenderer* InProcessViewRenderer::FromWebContents(
    content::WebContents* contents) {
  return UserData::GetInstance(contents);
}

void InProcessViewRenderer::WebContentsGone() {
  web_contents_ = NULL;
  compositor_ = NULL;
}

// static
void InProcessViewRenderer::CalculateTileMemoryPolicy() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kTileMemoryMultiplier)) {
    std::string string_value =
        cl->GetSwitchValueASCII(switches::kTileMemoryMultiplier);
    int int_value = 0;
    if (base::StringToInt(string_value, &int_value) &&
        int_value >= 2 && int_value <= 50) {
      g_memory_multiplier = int_value;
    }
  }

  if (cl->HasSwitch(switches::kNumGrallocBuffersPerWebview)) {
    std::string string_value =
        cl->GetSwitchValueASCII(switches::kNumGrallocBuffersPerWebview);
    int int_value = 0;
    if (base::StringToInt(string_value, &int_value) &&
        int_value >= 50 && int_value <= 500) {
      g_num_gralloc_limit = int_value;
    }
  }

  const char kDefaultTileSize[] = "384";
  if (!cl->HasSwitch(switches::kDefaultTileWidth))
    cl->AppendSwitchASCII(switches::kDefaultTileWidth, kDefaultTileSize);

  if (!cl->HasSwitch(switches::kDefaultTileHeight))
    cl->AppendSwitchASCII(switches::kDefaultTileHeight, kDefaultTileSize);
}

bool InProcessViewRenderer::RequestProcessGL() {
  return client_->RequestDrawGL(NULL);
}

void InProcessViewRenderer::TrimMemory(int level) {
  // Constants from Android ComponentCallbacks2.
  enum {
    TRIM_MEMORY_RUNNING_LOW = 10,
    TRIM_MEMORY_UI_HIDDEN = 20,
    TRIM_MEMORY_BACKGROUND = 40,
  };

  // Not urgent enough. TRIM_MEMORY_UI_HIDDEN is treated specially because
  // it does not indicate memory pressure, but merely that the app is
  // backgrounded.
  if (level < TRIM_MEMORY_RUNNING_LOW || level == TRIM_MEMORY_UI_HIDDEN)
    return;

  // Nothing to drop.
  if (!attached_to_window_ || !hardware_initialized_ || !compositor_)
    return;

  // Do not release resources on view we expect to get DrawGL soon.
  if (level < TRIM_MEMORY_BACKGROUND) {
    client_->UpdateGlobalVisibleRect();
    if (view_visible_ && window_visible_ &&
        !cached_global_visible_rect_.IsEmpty()) {
      return;
    }
  }

  if (!eglGetCurrentContext()) {
    NOTREACHED();
    return;
  }

  // Just set the memory limit to 0 and drop all tiles. This will be reset to
  // normal levels in the next DrawGL call.
  content::SynchronousCompositorMemoryPolicy policy;
  policy.bytes_limit = 0;
  policy.num_resources_limit = 0;
  if (memory_policy_ == policy)
    return;

  TRACE_EVENT0("android_webview", "InProcessViewRenderer::TrimMemory");
  ScopedAppGLStateRestore state_restore(
      ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT);
  g_service.Get()->RunTasks();
  ScopedAllowGL allow_gl;

  SetMemoryPolicy(policy);
  ForceFakeCompositeSW();
}

void InProcessViewRenderer::SetMemoryPolicy(
    content::SynchronousCompositorMemoryPolicy& new_policy) {
  if (memory_policy_ == new_policy)
    return;

  memory_policy_ = new_policy;
  compositor_->SetMemoryPolicy(memory_policy_);
}

void InProcessViewRenderer::UpdateCachedGlobalVisibleRect() {
  client_->UpdateGlobalVisibleRect();
}

bool InProcessViewRenderer::OnDraw(jobject java_canvas,
                                   bool is_hardware_canvas,
                                   const gfx::Vector2d& scroll,
                                   const gfx::Rect& clip) {
  scroll_at_start_of_frame_  = scroll;
  if (clear_view_)
    return false;
  if (is_hardware_canvas && attached_to_window_ && HardwareEnabled()) {
    // We should be performing a hardware draw here. If we don't have the
    // comositor yet or if RequestDrawGL fails, it means we failed this draw and
    // thus return false here to clear to background color for this draw.
    return compositor_ && client_->RequestDrawGL(java_canvas);
  }
  // Perform a software draw
  return DrawSWInternal(java_canvas, clip);
}

bool InProcessViewRenderer::InitializeHwDraw() {
  TRACE_EVENT0("android_webview", "InitializeHwDraw");
  DCHECK(!gl_surface_);
  gl_surface_ = new AwGLSurface;
  if (!g_service.Get()) {
    g_service.Get() = new DeferredGpuCommandService;
    content::SynchronousCompositor::SetGpuService(g_service.Get());
  }
  hardware_failed_ = !compositor_->InitializeHwDraw(gl_surface_);
  hardware_initialized_ = true;

  if (hardware_failed_)
    gl_surface_ = NULL;

  return !hardware_failed_;
}

void InProcessViewRenderer::DrawGL(AwDrawGLInfo* draw_info) {
  TRACE_EVENT0("android_webview", "InProcessViewRenderer::DrawGL");

  manager_key_ = g_view_renderer_manager.Get().DidDrawGL(manager_key_, this);

  // We need to watch if the current Android context has changed and enforce
  // a clean-up in the compositor.
  EGLContext current_context = eglGetCurrentContext();
  if (!current_context) {
    TRACE_EVENT_INSTANT0(
        "android_webview", "EarlyOut_NullEGLContext", TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  ScopedAppGLStateRestore state_restore(ScopedAppGLStateRestore::MODE_DRAW);
  if (g_service.Get())
    g_service.Get()->RunTasks();
  ScopedAllowGL allow_gl;

  if (!attached_to_window_) {
    TRACE_EVENT_INSTANT0(
        "android_webview", "EarlyOut_NotAttached", TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  if (draw_info->mode == AwDrawGLInfo::kModeProcess) {
    TRACE_EVENT_INSTANT0(
        "android_webview", "EarlyOut_ModeProcess", TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  if (compositor_ && !hardware_initialized_) {
    if (InitializeHwDraw()) {
      last_egl_context_ = current_context;
    } else {
      TRACE_EVENT_INSTANT0(
          "android_webview", "EarlyOut_HwInitFail", TRACE_EVENT_SCOPE_THREAD);
      LOG(ERROR) << "WebView hardware initialization failed";
      return;
    }
  }

  UpdateCachedGlobalVisibleRect();
  if (cached_global_visible_rect_.IsEmpty()) {
    TRACE_EVENT_INSTANT0("android_webview",
                         "EarlyOut_EmptyVisibleRect",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  if (last_egl_context_ != current_context) {
    // TODO(boliu): Handle context lost
    TRACE_EVENT_INSTANT0(
        "android_webview", "EGLContextChanged", TRACE_EVENT_SCOPE_THREAD);
  }

  if (!compositor_) {
    TRACE_EVENT_INSTANT0(
        "android_webview", "EarlyOut_NoCompositor", TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  // DrawGL may be called without OnDraw, so cancel |fallback_tick_| here as
  // well just to be safe.
  fallback_tick_.Cancel();

  // Update memory budget. This will no-op in compositor if the policy has not
  // changed since last draw.
  content::SynchronousCompositorMemoryPolicy policy;
  policy.bytes_limit = g_memory_multiplier * kBytesPerPixel *
                       cached_global_visible_rect_.width() *
                       cached_global_visible_rect_.height();
  // Round up to a multiple of kMemoryAllocationStep.
  policy.bytes_limit =
      (policy.bytes_limit / kMemoryAllocationStep + 1) * kMemoryAllocationStep;
  policy.num_resources_limit = g_num_gralloc_limit;
  SetMemoryPolicy(policy);

  DCHECK(gl_surface_);
  gl_surface_->SetBackingFrameBufferObject(
      state_restore.framebuffer_binding_ext());

  gfx::Transform transform;
  transform.matrix().setColMajorf(draw_info->transform);
  transform.Translate(scroll_at_start_of_frame_.x(),
                      scroll_at_start_of_frame_.y());
  gfx::Rect clip_rect(draw_info->clip_left,
                      draw_info->clip_top,
                      draw_info->clip_right - draw_info->clip_left,
                      draw_info->clip_bottom - draw_info->clip_top);

  // Assume we always draw the full visible rect if we are drawing into a layer.
  bool drew_full_visible_rect = true;

  gfx::Rect viewport_rect;
  if (!draw_info->is_layer) {
    viewport_rect = cached_global_visible_rect_;
    clip_rect.Intersect(viewport_rect);
    drew_full_visible_rect = clip_rect.Contains(viewport_rect);
  } else {
    viewport_rect = clip_rect;
  }

  block_invalidates_ = true;
  // TODO(joth): Check return value.
  compositor_->DemandDrawHw(gfx::Size(draw_info->width, draw_info->height),
                            transform,
                            viewport_rect,
                            clip_rect,
                            state_restore.stencil_enabled());
  block_invalidates_ = false;
  gl_surface_->ResetBackingFrameBufferObject();

  EnsureContinuousInvalidation(draw_info, !drew_full_visible_rect);
}

void InProcessViewRenderer::SetGlobalVisibleRect(
    const gfx::Rect& visible_rect) {
  cached_global_visible_rect_ = visible_rect;
}

bool InProcessViewRenderer::DrawSWInternal(jobject java_canvas,
                                           const gfx::Rect& clip) {
  if (clip.IsEmpty()) {
    TRACE_EVENT_INSTANT0(
        "android_webview", "EarlyOut_EmptyClip", TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  if (!compositor_) {
    TRACE_EVENT_INSTANT0(
        "android_webview", "EarlyOut_NoCompositor", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  return RenderViaAuxilaryBitmapIfNeeded(
      java_canvas,
      java_helper_,
      scroll_at_start_of_frame_,
      clip,
      base::Bind(&InProcessViewRenderer::CompositeSW,
                 base::Unretained(this)),
      web_contents_);
}

// static
bool InProcessViewRenderer::RenderViaAuxilaryBitmapIfNeeded(
      jobject java_canvas,
      BrowserViewRenderer::JavaHelper* java_helper,
      const gfx::Vector2d& scroll_correction,
      const gfx::Rect& clip,
      InProcessViewRenderer::RenderMethod render_source,
      void* owner_key) {
  TRACE_EVENT0("android_webview",
               "InProcessViewRenderer::RenderViaAuxilaryBitmapIfNeeded");

  JNIEnv* env = AttachCurrentThread();
  ScopedPixelAccess auto_release_pixels(env, java_canvas);
  AwPixelInfo* pixels = auto_release_pixels.pixels();
  if (pixels && pixels->state) {
    skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(
        SkCanvasStateUtils::CreateFromCanvasState(pixels->state));

    // Workarounds for http://crbug.com/271096: SW draw only supports
    // translate & scale transforms, and a simple rectangular clip.
    if (canvas && (!canvas->getTotalClip().isRect() ||
          (canvas->getTotalMatrix().getType() &
           ~(SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask)))) {
      canvas.clear();
    }
    if (canvas) {
      canvas->translate(scroll_correction.x(), scroll_correction.y());
      return render_source.Run(canvas.get());
    }
  }

  // Render into an auxiliary bitmap if pixel info is not available.
  ScopedJavaLocalRef<jobject> jcanvas(env, java_canvas);
  TRACE_EVENT0("android_webview", "RenderToAuxBitmap");
  ScopedJavaLocalRef<jobject> jbitmap(java_helper->CreateBitmap(
      env, clip.width(), clip.height(), jcanvas, owner_key));
  if (!jbitmap.obj()) {
    TRACE_EVENT_INSTANT0("android_webview",
                         "EarlyOut_BitmapAllocFail",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (!RasterizeIntoBitmap(env, jbitmap,
                           clip.x() - scroll_correction.x(),
                           clip.y() - scroll_correction.y(),
                           render_source)) {
    TRACE_EVENT_INSTANT0("android_webview",
                         "EarlyOut_RasterizeFail",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  java_helper->DrawBitmapIntoCanvas(env, jbitmap, jcanvas,
                                    clip.x(), clip.y());
  return true;
}

skia::RefPtr<SkPicture> InProcessViewRenderer::CapturePicture(int width,
                                                              int height) {
  TRACE_EVENT0("android_webview", "InProcessViewRenderer::CapturePicture");

  // Return empty Picture objects for empty SkPictures.
  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  if (width <= 0 || height <= 0) {
    return picture;
  }

  // Reset scroll back to the origin, will go back to the old
  // value when scroll_reset is out of scope.
  base::AutoReset<gfx::Vector2dF> scroll_reset(&scroll_offset_dip_,
                                               gfx::Vector2d());

  SkCanvas* rec_canvas = picture->beginRecording(width, height, 0);
  if (compositor_)
    CompositeSW(rec_canvas);
  picture->endRecording();
  return picture;
}

void InProcessViewRenderer::EnableOnNewPicture(bool enabled) {
  on_new_picture_enable_ = enabled;
  EnsureContinuousInvalidation(NULL, false);
}

void InProcessViewRenderer::ClearView() {
  TRACE_EVENT_INSTANT0("android_webview",
                       "InProcessViewRenderer::ClearView",
                       TRACE_EVENT_SCOPE_THREAD);
  if (clear_view_)
    return;

  clear_view_ = true;
  // Always invalidate ignoring the compositor to actually clear the webview.
  EnsureContinuousInvalidation(NULL, true);
}

void InProcessViewRenderer::SetIsPaused(bool paused) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "InProcessViewRenderer::SetIsPaused",
                       TRACE_EVENT_SCOPE_THREAD,
                       "paused",
                       paused);
  is_paused_ = paused;
  EnsureContinuousInvalidation(NULL, false);
}

void InProcessViewRenderer::SetViewVisibility(bool view_visible) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "InProcessViewRenderer::SetViewVisibility",
                       TRACE_EVENT_SCOPE_THREAD,
                       "view_visible",
                       view_visible);
  view_visible_ = view_visible;
}

void InProcessViewRenderer::SetWindowVisibility(bool window_visible) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "InProcessViewRenderer::SetWindowVisibility",
                       TRACE_EVENT_SCOPE_THREAD,
                       "window_visible",
                       window_visible);
  window_visible_ = window_visible;
  EnsureContinuousInvalidation(NULL, false);
}

void InProcessViewRenderer::OnSizeChanged(int width, int height) {
  TRACE_EVENT_INSTANT2("android_webview",
                       "InProcessViewRenderer::OnSizeChanged",
                       TRACE_EVENT_SCOPE_THREAD,
                       "width",
                       width,
                       "height",
                       height);
  width_ = width;
  height_ = height;
}

void InProcessViewRenderer::OnAttachedToWindow(int width, int height) {
  TRACE_EVENT2("android_webview",
               "InProcessViewRenderer::OnAttachedToWindow",
               "width",
               width,
               "height",
               height);
  attached_to_window_ = true;
  width_ = width;
  height_ = height;
}

void InProcessViewRenderer::OnDetachedFromWindow() {
  TRACE_EVENT0("android_webview",
               "InProcessViewRenderer::OnDetachedFromWindow");

  NoLongerExpectsDrawGL();
  if (hardware_initialized_) {
    DCHECK(compositor_);

    ScopedAppGLStateRestore state_restore(
        ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT);
    g_service.Get()->RunTasks();
    ScopedAllowGL allow_gl;
    compositor_->ReleaseHwDraw();
    hardware_initialized_ = false;
  }

  gl_surface_ = NULL;
  attached_to_window_ = false;
}

bool InProcessViewRenderer::IsAttachedToWindow() {
  return attached_to_window_;
}

bool InProcessViewRenderer::IsVisible() {
  // Ignore |window_visible_| if |attached_to_window_| is false.
  return view_visible_ && (!attached_to_window_ || window_visible_);
}

gfx::Rect InProcessViewRenderer::GetScreenRect() {
  return gfx::Rect(client_->GetLocationOnScreen(), gfx::Size(width_, height_));
}

void InProcessViewRenderer::DidInitializeCompositor(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT0("android_webview",
               "InProcessViewRenderer::DidInitializeCompositor");
  DCHECK(compositor && compositor_ == NULL);
  compositor_ = compositor;
  hardware_initialized_ = false;
  hardware_failed_ = false;
}

void InProcessViewRenderer::DidDestroyCompositor(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT0("android_webview",
               "InProcessViewRenderer::DidDestroyCompositor");
  DCHECK(compositor_ == compositor);

  // This can fail if Apps call destroy while the webview is still attached
  // to the view tree. This is an illegal operation that will lead to leaks.
  // Log for now. Consider a proper fix if this becomes a problem.
  LOG_IF(ERROR, hardware_initialized_)
      << "Destroy called before OnDetachedFromWindow. May Leak GL resources";
  compositor_ = NULL;
}

void InProcessViewRenderer::SetContinuousInvalidate(bool invalidate) {
  if (compositor_needs_continuous_invalidate_ == invalidate)
    return;

  TRACE_EVENT_INSTANT1("android_webview",
                       "InProcessViewRenderer::SetContinuousInvalidate",
                       TRACE_EVENT_SCOPE_THREAD,
                       "invalidate",
                       invalidate);
  compositor_needs_continuous_invalidate_ = invalidate;
  EnsureContinuousInvalidation(NULL, false);
}

void InProcessViewRenderer::SetDipScale(float dip_scale) {
  dip_scale_ = dip_scale;
  CHECK(dip_scale_ > 0);
}

gfx::Vector2d InProcessViewRenderer::max_scroll_offset() const {
  DCHECK_GT(dip_scale_, 0);
  return gfx::ToCeiledVector2d(gfx::ScaleVector2d(
      max_scroll_offset_dip_, dip_scale_ * page_scale_factor_));
}

void InProcessViewRenderer::ScrollTo(gfx::Vector2d scroll_offset) {
  gfx::Vector2d max_offset = max_scroll_offset();
  gfx::Vector2dF scroll_offset_dip;
  // To preserve the invariant that scrolling to the maximum physical pixel
  // value also scrolls to the maximum dip pixel value we transform the physical
  // offset into the dip offset by using a proportion (instead of dividing by
  // dip_scale * page_scale_factor).
  if (max_offset.x()) {
    scroll_offset_dip.set_x((scroll_offset.x() * max_scroll_offset_dip_.x()) /
                            max_offset.x());
  }
  if (max_offset.y()) {
    scroll_offset_dip.set_y((scroll_offset.y() * max_scroll_offset_dip_.y()) /
                            max_offset.y());
  }

  DCHECK_LE(0, scroll_offset_dip.x());
  DCHECK_LE(0, scroll_offset_dip.y());
  DCHECK_LE(scroll_offset_dip.x(), max_scroll_offset_dip_.x());
  DCHECK_LE(scroll_offset_dip.y(), max_scroll_offset_dip_.y());

  if (scroll_offset_dip_ == scroll_offset_dip)
    return;

  scroll_offset_dip_ = scroll_offset_dip;

  if (compositor_)
    compositor_->DidChangeRootLayerScrollOffset();
}

void InProcessViewRenderer::DidUpdateContent() {
  TRACE_EVENT_INSTANT0("android_webview",
                       "InProcessViewRenderer::DidUpdateContent",
                       TRACE_EVENT_SCOPE_THREAD);
  clear_view_ = false;
  EnsureContinuousInvalidation(NULL, false);
  if (on_new_picture_enable_)
    client_->OnNewPicture();
}

void InProcessViewRenderer::SetMaxRootLayerScrollOffset(
    gfx::Vector2dF new_value_dip) {
  DCHECK_GT(dip_scale_, 0);

  max_scroll_offset_dip_ = new_value_dip;
  DCHECK_LE(0, max_scroll_offset_dip_.x());
  DCHECK_LE(0, max_scroll_offset_dip_.y());

  client_->SetMaxContainerViewScrollOffset(max_scroll_offset());
}

void InProcessViewRenderer::SetTotalRootLayerScrollOffset(
    gfx::Vector2dF scroll_offset_dip) {
  // TOOD(mkosiba): Add a DCHECK to say that this does _not_ get called during
  // DrawGl when http://crbug.com/249972 is fixed.
  if (scroll_offset_dip_ == scroll_offset_dip)
    return;

  scroll_offset_dip_ = scroll_offset_dip;

  gfx::Vector2d max_offset = max_scroll_offset();
  gfx::Vector2d scroll_offset;
  // For an explanation as to why this is done this way see the comment in
  // InProcessViewRenderer::ScrollTo.
  if (max_scroll_offset_dip_.x()) {
    scroll_offset.set_x((scroll_offset_dip.x() * max_offset.x()) /
                        max_scroll_offset_dip_.x());
  }

  if (max_scroll_offset_dip_.y()) {
    scroll_offset.set_y((scroll_offset_dip.y() * max_offset.y()) /
                        max_scroll_offset_dip_.y());
  }

  DCHECK(0 <= scroll_offset.x());
  DCHECK(0 <= scroll_offset.y());
  // Disabled because the conditions are being violated while running
  // AwZoomTest.testMagnification, see http://crbug.com/340648
  // DCHECK(scroll_offset.x() <= max_offset.x());
  // DCHECK(scroll_offset.y() <= max_offset.y());

  client_->ScrollContainerViewTo(scroll_offset);
}

gfx::Vector2dF InProcessViewRenderer::GetTotalRootLayerScrollOffset() {
  return scroll_offset_dip_;
}

bool InProcessViewRenderer::IsExternalFlingActive() const {
  return client_->IsFlingActive();
}

void InProcessViewRenderer::SetRootLayerPageScaleFactorAndLimits(
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  page_scale_factor_ = page_scale_factor;
  DCHECK_GT(page_scale_factor_, 0);
  client_->SetPageScaleFactorAndLimits(
      page_scale_factor, min_page_scale_factor, max_page_scale_factor);
}

void InProcessViewRenderer::SetRootLayerScrollableSize(
    gfx::SizeF scrollable_size) {
  client_->SetContentsSize(scrollable_size);
}

void InProcessViewRenderer::DidOverscroll(
    gfx::Vector2dF accumulated_overscroll,
    gfx::Vector2dF latest_overscroll_delta,
    gfx::Vector2dF current_fling_velocity) {
  const float physical_pixel_scale = dip_scale_ * page_scale_factor_;
  if (accumulated_overscroll == latest_overscroll_delta)
    overscroll_rounding_error_ = gfx::Vector2dF();
  gfx::Vector2dF scaled_overscroll_delta =
      gfx::ScaleVector2d(latest_overscroll_delta, physical_pixel_scale);
  gfx::Vector2d rounded_overscroll_delta = gfx::ToRoundedVector2d(
      scaled_overscroll_delta + overscroll_rounding_error_);
  overscroll_rounding_error_ =
      scaled_overscroll_delta - rounded_overscroll_delta;
  client_->DidOverscroll(rounded_overscroll_delta);
}

void InProcessViewRenderer::EnsureContinuousInvalidation(
    AwDrawGLInfo* draw_info,
    bool invalidate_ignore_compositor) {
  // This method should be called again when any of these conditions change.
  bool need_invalidate =
      compositor_needs_continuous_invalidate_ || invalidate_ignore_compositor;
  if (!need_invalidate || block_invalidates_)
    return;

  // Always call view invalidate. We rely the Android framework to ignore the
  // invalidate when it's not needed such as when view is not visible.
  if (draw_info) {
    draw_info->dirty_left = cached_global_visible_rect_.x();
    draw_info->dirty_top = cached_global_visible_rect_.y();
    draw_info->dirty_right = cached_global_visible_rect_.right();
    draw_info->dirty_bottom = cached_global_visible_rect_.bottom();
    draw_info->status_mask |= AwDrawGLInfo::kStatusMaskDraw;
  } else {
    client_->PostInvalidate();
  }

  // Stop fallback ticks when one of these is true.
  // 1) Webview is paused. Also need to check we are not in clear view since
  // paused, offscreen still expect clear view to recover.
  // 2) If we are attached to window and the window is not visible (eg when
  // app is in the background). We are sure in this case the webview is used
  // "on-screen" but that updates are not needed when in the background.
  bool throttle_fallback_tick =
      (is_paused_ && !clear_view_) || (attached_to_window_ && !window_visible_);
  if (throttle_fallback_tick)
    return;

  block_invalidates_ = compositor_needs_continuous_invalidate_;

  // Unretained here is safe because the callback is cancelled when
  // |fallback_tick_| is destroyed.
  fallback_tick_.Reset(base::Bind(&InProcessViewRenderer::FallbackTickFired,
                                  base::Unretained(this)));

  // No need to reschedule fallback tick if compositor does not need to be
  // ticked. This can happen if this is reached because
  // invalidate_ignore_compositor is true.
  if (compositor_needs_continuous_invalidate_) {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        fallback_tick_.callback(),
        base::TimeDelta::FromMilliseconds(
            kFallbackTickTimeoutInMilliseconds));
  }
}

void InProcessViewRenderer::FallbackTickFired() {
  TRACE_EVENT1("android_webview",
               "InProcessViewRenderer::FallbackTickFired",
               "compositor_needs_continuous_invalidate_",
               compositor_needs_continuous_invalidate_);

  // This should only be called if OnDraw or DrawGL did not come in time, which
  // means block_invalidates_ must still be true.
  DCHECK(block_invalidates_);
  if (compositor_needs_continuous_invalidate_ && compositor_)
    ForceFakeCompositeSW();
}

void InProcessViewRenderer::ForceFakeCompositeSW() {
  DCHECK(compositor_);
  SkBitmapDevice device(SkBitmap::kARGB_8888_Config, 1, 1);
  SkCanvas canvas(&device);
  CompositeSW(&canvas);
}

bool InProcessViewRenderer::CompositeSW(SkCanvas* canvas) {
  DCHECK(compositor_);

  fallback_tick_.Cancel();
  block_invalidates_ = true;
  bool result = compositor_->DemandDrawSw(canvas);
  block_invalidates_ = false;
  EnsureContinuousInvalidation(NULL, false);
  return result;
}

std::string InProcessViewRenderer::ToString(AwDrawGLInfo* draw_info) const {
  std::string str;
  base::StringAppendF(&str, "is_paused: %d ", is_paused_);
  base::StringAppendF(&str, "view_visible: %d ", view_visible_);
  base::StringAppendF(&str, "window_visible: %d ", window_visible_);
  base::StringAppendF(&str, "dip_scale: %f ", dip_scale_);
  base::StringAppendF(&str, "page_scale_factor: %f ", page_scale_factor_);
  base::StringAppendF(&str,
                      "compositor_needs_continuous_invalidate: %d ",
                      compositor_needs_continuous_invalidate_);
  base::StringAppendF(&str, "block_invalidates: %d ", block_invalidates_);
  base::StringAppendF(&str, "view width height: [%d %d] ", width_, height_);
  base::StringAppendF(&str, "attached_to_window: %d ", attached_to_window_);
  base::StringAppendF(&str, "hardware_initialized: %d ", hardware_initialized_);
  base::StringAppendF(&str, "hardware_failed: %d ", hardware_failed_);
  base::StringAppendF(&str,
                      "global visible rect: %s ",
                      cached_global_visible_rect_.ToString().c_str());
  base::StringAppendF(&str,
                      "scroll_at_start_of_frame: %s ",
                      scroll_at_start_of_frame_.ToString().c_str());
  base::StringAppendF(
      &str, "scroll_offset_dip: %s ", scroll_offset_dip_.ToString().c_str());
  base::StringAppendF(&str,
                      "overscroll_rounding_error_: %s ",
                      overscroll_rounding_error_.ToString().c_str());
  base::StringAppendF(
      &str, "on_new_picture_enable: %d ", on_new_picture_enable_);
  base::StringAppendF(&str, "clear_view: %d ", clear_view_);
  if (draw_info) {
    base::StringAppendF(&str,
                        "clip left top right bottom: [%d %d %d %d] ",
                        draw_info->clip_left,
                        draw_info->clip_top,
                        draw_info->clip_right,
                        draw_info->clip_bottom);
    base::StringAppendF(&str,
                        "surface width height: [%d %d] ",
                        draw_info->width,
                        draw_info->height);
    base::StringAppendF(&str, "is_layer: %d ", draw_info->is_layer);
  }
  return str;
}

}  // namespace android_webview
