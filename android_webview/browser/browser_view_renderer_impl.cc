// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/browser_view_renderer_impl.h"

#include <android/bitmap.h>

#include "android_webview/common/renderer_picture_map.h"
#include "android_webview/public/browser/draw_gl.h"
#include "android_webview/public/browser/draw_sw.h"
#include "base/android/jni_android.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cc/layers/layer.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/vector2d_f.h"
#include "ui/gl/gl_bindings.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::Compositor;
using content::ContentViewCore;

namespace android_webview {

namespace {

// Provides software rendering functions from the Android glue layer.
// Allows preventing extra copies of data when rendering.
AwDrawSWFunctionTable* g_sw_draw_functions = NULL;

// Tells if the Skia library versions in Android and Chromium are compatible.
// If they are then it's possible to pass Skia objects like SkPictures to the
// Android glue layer via the SW rendering functions.
// If they are not, then additional copies and rasterizations are required
// as a fallback mechanism, which will have an important performance impact.
bool g_is_skia_version_compatible = false;

typedef base::Callback<bool(SkCanvas*)> RenderMethod;

static bool RasterizeIntoBitmap(JNIEnv* env,
                                const JavaRef<jobject>& jbitmap,
                                int scroll_x,
                                int scroll_y,
                                const RenderMethod& renderer) {
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

    SkDevice device(bitmap);
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

const void* kUserDataKey = &kUserDataKey;

}  // namespace

class BrowserViewRendererImpl::UserData : public content::WebContents::Data {
 public:
  UserData(BrowserViewRendererImpl* ptr) : instance_(ptr) {}
  virtual ~UserData() {
    instance_->WebContentsGone();
  }

  static BrowserViewRendererImpl* GetInstance(content::WebContents* contents) {
    if (!contents)
      return NULL;
    UserData* data = reinterpret_cast<UserData*>(
        contents->GetUserData(kUserDataKey));
    return data ? data->instance_ : NULL;
  }

 private:
  BrowserViewRendererImpl* instance_;
};

// static
BrowserViewRendererImpl* BrowserViewRendererImpl::Create(
    BrowserViewRenderer::Client* client,
    JavaHelper* java_helper) {
  return new BrowserViewRendererImpl(client, java_helper);
}

// static
BrowserViewRendererImpl* BrowserViewRendererImpl::FromWebContents(
    content::WebContents* contents) {
  return UserData::GetInstance(contents);
}

BrowserViewRendererImpl::BrowserViewRendererImpl(
    BrowserViewRenderer::Client* client,
    JavaHelper* java_helper)
    : client_(client),
      java_helper_(java_helper),
      view_renderer_host_(new ViewRendererHost(NULL, this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(compositor_(Compositor::Create(this))),
      view_clip_layer_(cc::Layer::Create()),
      transform_layer_(cc::Layer::Create()),
      scissor_clip_layer_(cc::Layer::Create()),
      view_attached_(false),
      view_visible_(false),
      compositor_visible_(false),
      is_composite_pending_(false),
      dpi_scale_(1.0f),
      page_scale_(1.0f),
      on_new_picture_mode_(kOnNewPictureDisabled),
      last_frame_context_(NULL),
      web_contents_(NULL),
      update_frame_info_callback_(
          base::Bind(&BrowserViewRendererImpl::OnFrameInfoUpdated,
                     base::Unretained(ALLOW_THIS_IN_INITIALIZER_LIST(this)))) {

  DCHECK(java_helper);

  // Define the view hierarchy.
  transform_layer_->AddChild(view_clip_layer_);
  scissor_clip_layer_->AddChild(transform_layer_);
  compositor_->SetRootLayer(scissor_clip_layer_);

  RendererPictureMap::CreateInstance();
}

BrowserViewRendererImpl::~BrowserViewRendererImpl() {
  SetContents(NULL);
}

// static
void BrowserViewRendererImpl::SetAwDrawSWFunctionTable(
    AwDrawSWFunctionTable* table) {
  g_sw_draw_functions = table;
  g_is_skia_version_compatible =
      g_sw_draw_functions->is_skia_version_compatible(&SkGraphics::GetVersion);
  LOG_IF(WARNING, !g_is_skia_version_compatible)
      << "Skia versions are not compatible, rendering performance will suffer.";
}

void BrowserViewRendererImpl::SetContents(ContentViewCore* content_view_core) {
  // First remove association from the prior ContentViewCore / WebContents.
  if (web_contents_) {
    ContentViewCore* previous_content_view_core =
        ContentViewCore::FromWebContents(web_contents_);
    if (previous_content_view_core) {
      previous_content_view_core->RemoveFrameInfoCallback(
          update_frame_info_callback_);
    }
    web_contents_->SetUserData(kUserDataKey, NULL);
    DCHECK(!web_contents_);  // WebContentsGone should have been called.
  }

  if (!content_view_core)
    return;

  web_contents_ = content_view_core->GetWebContents();
  web_contents_->SetUserData(kUserDataKey, new UserData(this));
  view_renderer_host_->Observe(web_contents_);

  content_view_core->AddFrameInfoCallback(update_frame_info_callback_);
  dpi_scale_ = content_view_core->GetDpiScale();

  view_clip_layer_->RemoveAllChildren();
  view_clip_layer_->AddChild(content_view_core->GetLayer());
  Invalidate();
}

void BrowserViewRendererImpl::WebContentsGone() {
  web_contents_ = NULL;
}

void BrowserViewRendererImpl::DrawGL(AwDrawGLInfo* draw_info) {
  TRACE_EVENT0("android_webview", "BrowserViewRendererImpl::DrawGL");

  if (view_size_.IsEmpty() || !scissor_clip_layer_ ||
      draw_info->mode == AwDrawGLInfo::kModeProcess)
    return;

  DCHECK_EQ(draw_info->mode, AwDrawGLInfo::kModeDraw);

  SetCompositorVisibility(view_visible_);
  if (!compositor_visible_)
    return;

  // We need to watch if the current Android context has changed and enforce
  // a clean-up in the compositor.
  EGLContext current_context = eglGetCurrentContext();
  if (!current_context) {
    LOG(WARNING) << "No current context attached. Skipping composite.";
    return;
  }

  if (last_frame_context_ != current_context) {
    if (last_frame_context_)
      ResetCompositor();
    last_frame_context_ = current_context;
  }

  compositor_->SetWindowBounds(gfx::Size(draw_info->width, draw_info->height));

  if (draw_info->is_layer) {
    // When rendering into a separate layer no view clipping, transform,
    // scissoring or background transparency need to be handled.
    // The Android framework will composite us afterwards.
    compositor_->SetHasTransparentBackground(false);
    view_clip_layer_->SetMasksToBounds(false);
    transform_layer_->SetTransform(gfx::Transform());
    scissor_clip_layer_->SetMasksToBounds(false);
    scissor_clip_layer_->SetPosition(gfx::PointF());
    scissor_clip_layer_->SetBounds(gfx::Size());
    scissor_clip_layer_->SetSublayerTransform(gfx::Transform());

  } else {
    compositor_->SetHasTransparentBackground(true);

    gfx::Rect clip_rect(draw_info->clip_left, draw_info->clip_top,
                        draw_info->clip_right - draw_info->clip_left,
                        draw_info->clip_bottom - draw_info->clip_top);

    scissor_clip_layer_->SetPosition(clip_rect.origin());
    scissor_clip_layer_->SetBounds(clip_rect.size());
    scissor_clip_layer_->SetMasksToBounds(true);

    // The compositor clipping architecture enforces us to have the clip layer
    // as an ancestor of the area we want to clip, but this makes the transform
    // become relative to the clip area rather than the full surface. The clip
    // position offset needs to be undone before applying the transform.
    gfx::Transform undo_clip_position;
    undo_clip_position.Translate(-clip_rect.x(), -clip_rect.y());
    scissor_clip_layer_->SetSublayerTransform(undo_clip_position);

    gfx::Transform transform;
    transform.matrix().setColMajorf(draw_info->transform);

    // The scrolling values of the Android Framework affect the transformation
    // matrix. This needs to be undone to let the compositor handle scrolling.
    // TODO(leandrogracia): when scrolling becomes synchronous we should undo
    // or override the translation in the compositor, not the one coming from
    // the Android View System, as it could be out of bounds for overscrolling.
    transform.Translate(hw_rendering_scroll_.x(), hw_rendering_scroll_.y());
    transform_layer_->SetTransform(transform);

    view_clip_layer_->SetMasksToBounds(true);
  }

  compositor_->Composite();
  is_composite_pending_ = false;

  // The GL functor must ensure these are set to zero before returning.
  // Not setting them leads to graphical artifacts that can affect other apps.
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void BrowserViewRendererImpl::SetScrollForHWFrame(int x, int y) {
  hw_rendering_scroll_ = gfx::Point(x, y);
}

bool BrowserViewRendererImpl::DrawSW(jobject java_canvas,
                                     const gfx::Rect& clip) {
  TRACE_EVENT0("android_webview", "BrowserViewRendererImpl::DrawSW");

  if (clip.IsEmpty())
    return true;

  AwPixelInfo* pixels;
  JNIEnv* env = AttachCurrentThread();

  // Render into an auxiliary bitmap if pixel info is not available.
  if (!g_sw_draw_functions ||
      (pixels = g_sw_draw_functions->access_pixels(env, java_canvas)) == NULL) {
    ScopedJavaLocalRef<jobject> jbitmap(java_helper_->CreateBitmap(
        env, clip.width(), clip.height()));
    if (!jbitmap.obj())
      return false;

    if (!RasterizeIntoBitmap(env, jbitmap, clip.x(), clip.y(),
                             base::Bind(&BrowserViewRendererImpl::RenderSW,
                                        base::Unretained(this)))) {
      return false;
    }

    ScopedJavaLocalRef<jobject> jcanvas(env, java_canvas);
    java_helper_->DrawBitmapIntoCanvas(env, jbitmap, jcanvas);
    return true;
  }

  // Draw in a SkCanvas built over the pixel information.
  bool succeeded = false;
  {
    SkBitmap bitmap;
    bitmap.setConfig(static_cast<SkBitmap::Config>(pixels->config),
                     pixels->width,
                     pixels->height,
                     pixels->row_bytes);
    bitmap.setPixels(pixels->pixels);
    SkDevice device(bitmap);
    SkCanvas canvas(&device);
    SkMatrix matrix;
    for (int i = 0; i < 9; i++)
      matrix.set(i, pixels->matrix[i]);
    canvas.setMatrix(matrix);

    SkRegion clip;
    if (pixels->clip_region_size) {
      size_t bytes_read = clip.readFromMemory(pixels->clip_region);
      DCHECK_EQ(pixels->clip_region_size, bytes_read);
      canvas.setClipRegion(clip);
    } else {
      clip.setRect(SkIRect::MakeWH(pixels->width, pixels->height));
    }

    succeeded = RenderSW(&canvas);
  }

  g_sw_draw_functions->release_pixels(pixels);
  return succeeded;
}

ScopedJavaLocalRef<jobject> BrowserViewRendererImpl::CapturePicture() {
  skia::RefPtr<SkPicture> picture = GetLastCapturedPicture();
  if (!picture || !g_sw_draw_functions)
    return ScopedJavaLocalRef<jobject>();

  // Return empty Picture objects for empty SkPictures.
  JNIEnv* env = AttachCurrentThread();
  if (picture->width() <= 0 || picture->height() <= 0) {
    return java_helper_->RecordBitmapIntoPicture(
        env, ScopedJavaLocalRef<jobject>());
  }

  if (g_is_skia_version_compatible) {
    return ScopedJavaLocalRef<jobject>(env,
        g_sw_draw_functions->create_picture(env, picture->clone()));
  }

  // If Skia versions are not compatible, workaround it by rasterizing the
  // picture into a bitmap and drawing it into a new Java picture.
  ScopedJavaLocalRef<jobject> jbitmap(java_helper_->CreateBitmap(
      env, picture->width(), picture->height()));
  if (!jbitmap.obj())
    return ScopedJavaLocalRef<jobject>();

  if (!RasterizeIntoBitmap(env, jbitmap, 0, 0,
      base::Bind(&BrowserViewRendererImpl::RenderPicture,
                 base::Unretained(this)))) {
    return ScopedJavaLocalRef<jobject>();
  }

  return java_helper_->RecordBitmapIntoPicture(env, jbitmap);
}

void BrowserViewRendererImpl::EnableOnNewPicture(OnNewPictureMode mode) {
  on_new_picture_mode_ = mode;

  // TODO(leandrogracia): when SW rendering uses the compositor rather than
  // picture rasterization, send update the renderer side with the correct
  // listener state. (For now, we always leave render picture listener enabled).
  // render_view_host_ext_->EnableCapturePictureCallback(enabled);
  //DCHECK(view_renderer_host_);
  //view_renderer_host_->EnableCapturePictureCallback(
  //    on_new_picture_mode_ == kOnNewPictureEnabled);
}

void BrowserViewRendererImpl::OnVisibilityChanged(bool view_visible,
                                                  bool window_visible) {
  view_visible_ = window_visible && view_visible;
  Invalidate();
}

void BrowserViewRendererImpl::OnSizeChanged(int width, int height) {
  view_size_ = gfx::Size(width, height);
  view_clip_layer_->SetBounds(view_size_);
}

void BrowserViewRendererImpl::OnAttachedToWindow(int width, int height) {
  view_attached_ = true;
  view_size_ = gfx::Size(width, height);
  view_clip_layer_->SetBounds(view_size_);
}

void BrowserViewRendererImpl::OnDetachedFromWindow() {
  view_attached_ = false;
  view_visible_ = false;
  SetCompositorVisibility(false);
}

bool BrowserViewRendererImpl::IsAttachedToWindow() {
  return view_attached_;
}

bool BrowserViewRendererImpl::IsViewVisible() {
  return view_visible_;
}

gfx::Rect BrowserViewRendererImpl::GetScreenRect() {
  return gfx::Rect(client_->GetLocationOnScreen(), view_size_);
}

void BrowserViewRendererImpl::ScheduleComposite() {
  TRACE_EVENT0("android_webview", "BrowserViewRendererImpl::ScheduleComposite");

  if (is_composite_pending_)
    return;

  is_composite_pending_ = true;
  Invalidate();
}

skia::RefPtr<SkPicture> BrowserViewRendererImpl::GetLastCapturedPicture() {
  // Use the latest available picture if the listener callback is enabled.
  skia::RefPtr<SkPicture> picture;
  if (on_new_picture_mode_ == kOnNewPictureEnabled)
    picture = RendererPictureMap::GetInstance()->GetRendererPicture(
        web_contents_->GetRoutingID());

  // If not available or not in listener mode get it synchronously.
  if (!picture) {
    view_renderer_host_->CapturePictureSync();
    picture = RendererPictureMap::GetInstance()->GetRendererPicture(
        web_contents_->GetRoutingID());
  }

  return picture;
}

void BrowserViewRendererImpl::OnPictureUpdated(int process_id,
                                               int render_view_id) {
  CHECK_EQ(web_contents_->GetRenderProcessHost()->GetID(), process_id);
  if (render_view_id != web_contents_->GetRoutingID())
    return;

  // TODO(leandrogracia): this can be made unconditional once software rendering
  // uses Ubercompositor. Until then this path is required for SW invalidations.
  if (on_new_picture_mode_ == kOnNewPictureEnabled)
    client_->OnNewPicture(CapturePicture());

  // TODO(leandrogracia): delete when sw rendering uses Ubercompositor.
  // Invalidation should be provided by the compositor only.
  Invalidate();
}

void BrowserViewRendererImpl::SetCompositorVisibility(bool visible) {
  if (compositor_visible_ != visible) {
    compositor_visible_ = visible;
    compositor_->SetVisible(compositor_visible_);
  }
}

void BrowserViewRendererImpl::ResetCompositor() {
  compositor_.reset(content::Compositor::Create(this));
  compositor_->SetRootLayer(scissor_clip_layer_);
}

void BrowserViewRendererImpl::Invalidate() {
  if (view_visible_)
    client_->Invalidate();

  // When not in invalidation-only mode onNewPicture will be triggered
  // from the OnPictureUpdated callback.
  if (on_new_picture_mode_ == kOnNewPictureInvalidationOnly)
    client_->OnNewPicture(ScopedJavaLocalRef<jobject>());
}

bool BrowserViewRendererImpl::RenderSW(SkCanvas* canvas) {
  float content_scale = dpi_scale_ * page_scale_;
  canvas->scale(content_scale, content_scale);

  // Clear to white any parts of the view not covered by the scaled contents.
  // TODO(leandrogracia): this should be automatically done by the SW rendering
  // path once multiple layers are supported.
  gfx::Size physical_content_size = gfx::ToCeiledSize(
      gfx::ScaleSize(content_size_css_, content_scale));
  if (physical_content_size.width() < view_size_.width() ||
      physical_content_size.height() < view_size_.height())
    canvas->clear(SK_ColorWHITE);

  // TODO(leandrogracia): use the appropriate SW rendering path when available
  // instead of abusing CapturePicture.
  return RenderPicture(canvas);
}

bool BrowserViewRendererImpl::RenderPicture(SkCanvas* canvas) {
  skia::RefPtr<SkPicture> picture = GetLastCapturedPicture();
  if (!picture)
    return false;

  picture->draw(canvas);
  return true;
}

void BrowserViewRendererImpl::OnFrameInfoUpdated(
    const gfx::SizeF& content_size,
    const gfx::Vector2dF& scroll_offset,
    float page_scale_factor) {
  page_scale_ = page_scale_factor;
  content_size_css_ = content_size;
}

}  // namespace android_webview
