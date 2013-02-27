// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/browser_view_renderer_impl.h"

#include <android/bitmap.h>
#include <sys/system_properties.h>

#include "android_webview/common/renderer_picture_map.h"
#include "android_webview/public/browser/draw_gl.h"
#include "android_webview/public/browser/draw_sw.h"
#include "base/android/jni_android.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cc/layer.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_bindings.h"

// TODO(leandrogracia): remove when crbug.com/164140 is closed.
// Borrowed from gl2ext.h. Cannot be included due to conflicts with
// gl_bindings.h and the EGL library methods (eglGetCurrentContext).
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

#ifndef GL_TEXTURE_BINDING_EXTERNAL_OES
#define GL_TEXTURE_BINDING_EXTERNAL_OES 0x8D67
#endif

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::Compositor;
using content::ContentViewCore;

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

}  // namespace

namespace android_webview {

// static
BrowserViewRendererImpl* BrowserViewRendererImpl::Create(
    BrowserViewRenderer::Client* client,
    JavaHelper* java_helper) {
  return new BrowserViewRendererImpl(client, java_helper);
}

BrowserViewRendererImpl::BrowserViewRendererImpl(
    BrowserViewRenderer::Client* client,
    JavaHelper* java_helper)
    : client_(client),
      java_helper_(java_helper),
      ALLOW_THIS_IN_INITIALIZER_LIST(compositor_(Compositor::Create(this))),
      view_clip_layer_(cc::Layer::create()),
      transform_layer_(cc::Layer::create()),
      scissor_clip_layer_(cc::Layer::create()),
      view_visible_(false),
      compositor_visible_(false),
      is_composite_pending_(false),
      dpi_scale_(1.0f),
      on_new_picture_mode_(kOnNewPictureDisabled),
      last_frame_context_(NULL),
      web_contents_(NULL) {
  DCHECK(java_helper);

  // Define the view hierarchy.
  transform_layer_->addChild(view_clip_layer_);
  scissor_clip_layer_->addChild(transform_layer_);
  compositor_->SetRootLayer(scissor_clip_layer_);

  RendererPictureMap::CreateInstance();
}

BrowserViewRendererImpl::~BrowserViewRendererImpl() {
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
  dpi_scale_ = content_view_core->GetDpiScale();
  web_contents_ = content_view_core->GetWebContents();
  if (!view_renderer_host_)
    view_renderer_host_.reset(new ViewRendererHost(web_contents_, this));
  else
    view_renderer_host_->Observe(web_contents_);

  view_clip_layer_->removeAllChildren();
  view_clip_layer_->addChild(content_view_core->GetLayer());
  Invalidate();
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

  // TODO(leandrogracia): remove when crbug.com/164140 is closed.
  // ---------------------------------------------------------------------------
  GLint texture_external_oes_binding;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &texture_external_oes_binding);

  GLint vertex_array_buffer_binding;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vertex_array_buffer_binding);

  GLint index_array_buffer_binding;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_array_buffer_binding);

  GLint pack_alignment;
  glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment);

  GLint unpack_alignment;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);

  struct {
    GLint enabled;
    GLint size;
    GLint type;
    GLint normalized;
    GLint stride;
    GLvoid* pointer;
  } vertex_attrib[3];

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(vertex_attrib); ++i) {
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED,
                        &vertex_attrib[i].enabled);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE,
                        &vertex_attrib[i].size);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE,
                        &vertex_attrib[i].type);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                        &vertex_attrib[i].normalized);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE,
                        &vertex_attrib[i].stride);
    glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER,
                        &vertex_attrib[i].pointer);
  }

  GLboolean depth_test;
  glGetBooleanv(GL_DEPTH_TEST, &depth_test);

  GLboolean cull_face;
  glGetBooleanv(GL_CULL_FACE, &cull_face);

  GLboolean color_mask[4];
  glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);

  GLboolean blend_enabled;
  glGetBooleanv(GL_BLEND, &blend_enabled);

  GLint blend_src_rgb;
  glGetIntegerv(GL_BLEND_SRC_RGB, &blend_src_rgb);

  GLint blend_src_alpha;
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src_alpha);

  GLint blend_dest_rgb;
  glGetIntegerv(GL_BLEND_DST_RGB, &blend_dest_rgb);

  GLint blend_dest_alpha;
  glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dest_alpha);

  GLint active_texture;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);

  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);

  GLboolean scissor_test;
  glGetBooleanv(GL_SCISSOR_TEST, &scissor_test);

  GLint scissor_box[4];
  glGetIntegerv(GL_SCISSOR_BOX, scissor_box);

  GLint current_program;
  glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
  // ---------------------------------------------------------------------------

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
    view_clip_layer_->setMasksToBounds(false);
    transform_layer_->setTransform(gfx::Transform());
    scissor_clip_layer_->setMasksToBounds(false);
    scissor_clip_layer_->setPosition(gfx::PointF());
    scissor_clip_layer_->setBounds(gfx::Size());
    scissor_clip_layer_->setSublayerTransform(gfx::Transform());

  } else {
    compositor_->SetHasTransparentBackground(true);

    gfx::Rect clip_rect(draw_info->clip_left, draw_info->clip_top,
                        draw_info->clip_right - draw_info->clip_left,
                        draw_info->clip_bottom - draw_info->clip_top);

    scissor_clip_layer_->setPosition(clip_rect.origin());
    scissor_clip_layer_->setBounds(clip_rect.size());
    scissor_clip_layer_->setMasksToBounds(true);

    // The compositor clipping architecture enforces us to have the clip layer
    // as an ancestor of the area we want to clip, but this makes the transform
    // become relative to the clip area rather than the full surface. The clip
    // position offset needs to be undone before applying the transform.
    gfx::Transform undo_clip_position;
    undo_clip_position.Translate(-clip_rect.x(), -clip_rect.y());
    scissor_clip_layer_->setSublayerTransform(undo_clip_position);

    gfx::Transform transform;
    transform.matrix().setColMajorf(draw_info->transform);

    // The scrolling values of the Android Framework affect the transformation
    // matrix. This needs to be undone to let the compositor handle scrolling.
    // TODO(leandrogracia): when scrolling becomes synchronous we should undo
    // or override the translation in the compositor, not the one coming from
    // the Android View System, as it could be out of bounds for overscrolling.
    transform.Translate(hw_rendering_scroll_.x(), hw_rendering_scroll_.y());
    transform_layer_->setTransform(transform);

    view_clip_layer_->setMasksToBounds(true);
  }

  compositor_->Composite();
  is_composite_pending_ = false;

  // TODO(leandrogracia): remove when crbug.com/164140 is closed.
  // ---------------------------------------------------------------------------
  char no_gl_restore_prop[PROP_VALUE_MAX];
  __system_property_get("webview.chromium_no_gl_restore", no_gl_restore_prop);
  if (!strcmp(no_gl_restore_prop, "true")) {
    LOG(WARNING) << "Android GL functor not restoring the previous GL state.";
  } else {
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_external_oes_binding);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_array_buffer_binding);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_array_buffer_binding);

    glPixelStorei(GL_PACK_ALIGNMENT, pack_alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(vertex_attrib); ++i) {
      glVertexAttribPointer(i, vertex_attrib[i].size,
          vertex_attrib[i].type, vertex_attrib[i].normalized,
          vertex_attrib[i].stride, vertex_attrib[i].pointer);

      if (vertex_attrib[i].enabled)
        glEnableVertexAttribArray(i);
      else
        glDisableVertexAttribArray(i);
    }

    if (depth_test)
      glEnable(GL_DEPTH_TEST);
    else
      glDisable(GL_DEPTH_TEST);

    if (cull_face)
      glEnable(GL_CULL_FACE);
    else
      glDisable(GL_CULL_FACE);

    glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);

    if (blend_enabled)
      glEnable(GL_BLEND);
    else
      glDisable(GL_BLEND);

    glBlendFuncSeparate(blend_src_rgb, blend_dest_rgb,
                        blend_src_alpha, blend_dest_alpha);

    glActiveTexture(active_texture);

    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    if (scissor_test)
      glEnable(GL_SCISSOR_TEST);
    else
      glDisable(GL_SCISSOR_TEST);

    glScissor(scissor_box[0], scissor_box[1], scissor_box[2],
              scissor_box[3]);

    glUseProgram(current_program);
  }
  // ---------------------------------------------------------------------------
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
  view_clip_layer_->setBounds(view_size_);
}

void BrowserViewRendererImpl::OnAttachedToWindow(int width, int height) {
  view_size_ = gfx::Size(width, height);
  view_clip_layer_->setBounds(view_size_);
}

void BrowserViewRendererImpl::OnDetachedFromWindow() {
  view_visible_ = false;
  SetCompositorVisibility(false);
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
    DCHECK(view_renderer_host_);
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
  // TODO(leandrogracia): once Ubercompositor is ready and we support software
  // rendering mode, we should avoid this as much as we can, ideally always.
  // This includes finding a proper replacement for onDraw calls in hardware
  // mode with software canvases. http://crbug.com/170086.
  return RenderPicture(canvas);
}

bool BrowserViewRendererImpl::RenderPicture(SkCanvas* canvas) {
  skia::RefPtr<SkPicture> picture = GetLastCapturedPicture();
  if (!picture)
    return false;

  // Correct device scale.
  canvas->scale(dpi_scale_, dpi_scale_);

  picture->draw(canvas);
  return true;
}

}  // namespace android_webview
