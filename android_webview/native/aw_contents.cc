// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents.h"

#include <sys/system_properties.h>

#include "android_webview/browser/aw_browser_main_parts.h"
#include "android_webview/browser/net_disk_cache_remover.h"
#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"
#include "android_webview/common/aw_hit_test_data.h"
#include "android_webview/common/renderer_picture_map.h"
#include "android_webview/native/aw_browser_dependency_factory.h"
#include "android_webview/native/aw_contents_io_thread_client_impl.h"
#include "android_webview/native/aw_web_contents_delegate.h"
#include "android_webview/native/state_serializer.h"
#include "android_webview/public/browser/draw_sw.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/pickle.h"
#include "base/supports_user_data.h"
#include "cc/layer.h"
#include "cc/picture_pile_impl.h"
#include "cc/rendering_stats.h"
#include "content/components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "jni/AwContents_jni.h"
#include "net/base/x509_certificate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
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
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using content::ContentViewCore;
using content::InterceptNavigationDelegate;
using content::WebContents;

extern "C" {
static AwDrawGLFunction DrawGLFunction;
static void DrawGLFunction(int view_context,
                           AwDrawGLInfo* draw_info,
                           void* spare) {
  // |view_context| is the value that was returned from the java
  // AwContents.onPrepareDrawGL; this cast must match the code there.
  reinterpret_cast<android_webview::AwContents*>(view_context)->DrawGL(
      draw_info);
}
}

namespace android_webview {

namespace {

AwDrawSWFunctionTable* g_draw_sw_functions = NULL;

const void* kAwContentsUserDataKey = &kAwContentsUserDataKey;

class AwContentsUserData : public base::SupportsUserData::Data {
 public:
  AwContentsUserData(AwContents* ptr) : contents_(ptr) {}

  static AwContents* GetContents(WebContents* web_contents) {
    if (!web_contents)
      return NULL;
    AwContentsUserData* data = reinterpret_cast<AwContentsUserData*>(
        web_contents->GetUserData(kAwContentsUserDataKey));
    return data ? data->contents_ : NULL;
  }

 private:
  AwContents* contents_;
};

}  // namespace

// static
AwContents* AwContents::FromWebContents(WebContents* web_contents) {
  return AwContentsUserData::GetContents(web_contents);
}

AwContents::AwContents(JNIEnv* env,
                       jobject obj,
                       jobject web_contents_delegate,
                       bool private_browsing)
    : java_ref_(env, obj),
      web_contents_delegate_(
          new AwWebContentsDelegate(env, web_contents_delegate)),
      view_visible_(false),
      compositor_visible_(false),
      is_composite_pending_(false),
      last_scroll_x_(0), last_scroll_y_(0),
      last_frame_context_(NULL) {
  RendererPictureMap::CreateInstance();
  android_webview::AwBrowserDependencyFactory* dependency_factory =
      android_webview::AwBrowserDependencyFactory::GetInstance();

  // TODO(joth): rather than create and set the WebContents here, expose the
  // factory method to java side and have that orchestrate the construction
  // order.
  SetWebContents(dependency_factory->CreateWebContents(private_browsing));
}

void AwContents::ResetCompositor() {
  compositor_.reset(content::Compositor::Create(this));
  if (scissor_clip_layer_.get())
    AttachLayerTree();
}

void AwContents::SetWebContents(content::WebContents* web_contents) {
  web_contents_.reset(web_contents);
  web_contents_->SetUserData(kAwContentsUserDataKey,
                             new AwContentsUserData(this));

  web_contents_->SetDelegate(web_contents_delegate_.get());
  render_view_host_ext_.reset(new AwRenderViewHostExt(web_contents_.get(),
                              this));
  ResetCompositor();
}

void AwContents::SetWebContents(JNIEnv* env, jobject obj, jint new_wc) {
  SetWebContents(reinterpret_cast<content::WebContents*>(new_wc));
}

AwContents::~AwContents() {
  DCHECK(AwContents::FromWebContents(web_contents_.get()) == this);
  web_contents_->RemoveUserData(kAwContentsUserDataKey);
  if (find_helper_.get())
    find_helper_->SetListener(NULL);
}

void AwContents::DrawGL(AwDrawGLInfo* draw_info) {

  TRACE_EVENT0("AwContents", "AwContents::DrawGL");

  if (!scissor_clip_layer_ || draw_info->mode == AwDrawGLInfo::kModeProcess)
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

  gfx::Transform transform;
  compositor_->SetWindowBounds(gfx::Size(draw_info->width, draw_info->height));

  if (draw_info->is_layer) {
    // When rendering into a separate layer no view clipping, transform,
    // scissoring or background transparency need to be handled.
    // The Android framework will composite us afterwards.
    compositor_->SetHasTransparentBackground(false);
    view_clip_layer_->setMasksToBounds(false);
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

    transform.matrix().setColMajorf(draw_info->transform);
    view_clip_layer_->setMasksToBounds(true);
  }

  // The scrolling values of the Android Framework affect the transformation
  // matrix. This needs to be undone to let the compositor handle scrolling.
  transform.Translate(last_scroll_x_, last_scroll_y_);
  transform_layer_->setTransform(transform);

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

    glColorMask(color_mask[0], color_mask[1], color_mask[2],
                       color_mask[3]);

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

bool AwContents::DrawSW(JNIEnv* env, jobject obj, jobject java_canvas) {
  scoped_refptr<cc::PicturePileImpl> picture =
      RendererPictureMap::GetInstance()->GetRendererPicture(
          web_contents_->GetRoutingID());
  if (!picture)
    return false;

  AwPixelInfo* pixels;
  if (!g_draw_sw_functions ||
      (pixels = g_draw_sw_functions->access_pixels(env, java_canvas)) == NULL) {
    // TODO(joth): Fall back to slow path rendering via temporary bitmap.
    return false;
  }

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
    for (int i = 0; i < 9; i++) {
      matrix.set(i, pixels->matrix[i]);
    }
    canvas.setMatrix(matrix);

    SkRegion clip;
    if (pixels->clip_region_size) {
      size_t bytes_read = clip.readFromMemory(pixels->clip_region);
      DCHECK_EQ(pixels->clip_region_size, bytes_read);
      canvas.setClipRegion(clip);
    } else {
      clip.setRect(SkIRect::MakeWH(pixels->width, pixels->height));
    }

    SkIRect sk_clip_rect = clip.getBounds();
    gfx::Rect clip_rect(sk_clip_rect.x(), sk_clip_rect.y(),
                        sk_clip_rect.width(), sk_clip_rect.height());

    cc::RenderingStats stats;
    picture->Raster(&canvas, clip_rect, 1.0, &stats);
  }

  g_draw_sw_functions->release_pixels(pixels);
  return true;
}

jint AwContents::GetWebContents(JNIEnv* env, jobject obj) {
  return reinterpret_cast<jint>(web_contents_.get());
}

void AwContents::DidInitializeContentViewCore(JNIEnv* env, jobject obj,
                                              jint content_view_core) {
  ContentViewCore* core = reinterpret_cast<ContentViewCore*>(content_view_core);
  DCHECK(core == ContentViewCore::FromWebContents(web_contents_.get()));

  // Ensures content keeps clipped within the view during transformations.
  view_clip_layer_ = cc::Layer::create();
  view_clip_layer_->setBounds(view_size_);
  view_clip_layer_->addChild(core->GetLayer());

  // Applies the transformation matrix.
  transform_layer_ = cc::Layer::create();
  transform_layer_->addChild(view_clip_layer_);

  // Ensures content is drawn within the scissor clip rect provided by the
  // Android framework.
  scissor_clip_layer_ = cc::Layer::create();
  scissor_clip_layer_->addChild(transform_layer_);

  AttachLayerTree();
}

void AwContents::AttachLayerTree() {
  DCHECK(scissor_clip_layer_.get());
  compositor_->SetRootLayer(scissor_clip_layer_);
  Invalidate();
}

void AwContents::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

// static
void SetAwDrawSWFunctionTable(JNIEnv* env, jclass, jint function_table) {
  g_draw_sw_functions =
      reinterpret_cast<AwDrawSWFunctionTable*>(function_table);
}

// static
jint GetAwDrawGLFunction(JNIEnv* env, jclass) {
  return reinterpret_cast<jint>(&DrawGLFunction);
}

namespace {
// |message| is passed as base::Owned, so it will automatically be deleted
// when the callback goes out of scope.
void DocumentHasImagesCallback(ScopedJavaGlobalRef<jobject>* message,
                               bool has_images) {
  Java_AwContents_onDocumentHasImagesResponse(AttachCurrentThread(),
                                              has_images,
                                              message->obj());
}
}  // namespace

void AwContents::DocumentHasImages(JNIEnv* env, jobject obj, jobject message) {
  ScopedJavaGlobalRef<jobject>* j_message = new ScopedJavaGlobalRef<jobject>();
  j_message->Reset(env, message);
  render_view_host_ext_->DocumentHasImages(
      base::Bind(&DocumentHasImagesCallback, base::Owned(j_message)));
}

namespace {
void GenerateMHTMLCallback(ScopedJavaGlobalRef<jobject>* callback,
                           const FilePath& path, int64 size) {
  JNIEnv* env = AttachCurrentThread();
  // Android files are UTF8, so the path conversion below is safe.
  Java_AwContents_generateMHTMLCallback(
      env,
      ConvertUTF8ToJavaString(env, path.AsUTF8Unsafe()).obj(),
      size, callback->obj());
}
}  // namespace

void AwContents::GenerateMHTML(JNIEnv* env, jobject obj,
                               jstring jpath, jobject callback) {
  ScopedJavaGlobalRef<jobject>* j_callback = new ScopedJavaGlobalRef<jobject>();
  j_callback->Reset(env, callback);
  web_contents_->GenerateMHTML(
      FilePath(ConvertJavaStringToUTF8(env, jpath)),
      base::Bind(&GenerateMHTMLCallback, base::Owned(j_callback)));
}

void AwContents::RunJavaScriptDialog(
    content::JavaScriptMessageType message_type,
    const GURL& origin_url,
    const string16& message_text,
    const string16& default_prompt_text,
    const ScopedJavaLocalRef<jobject>& js_result) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jurl(ConvertUTF8ToJavaString(
      env, origin_url.spec()));
  ScopedJavaLocalRef<jstring> jmessage(ConvertUTF16ToJavaString(
      env, message_text));
  switch (message_type) {
    case content::JAVASCRIPT_MESSAGE_TYPE_ALERT:
      Java_AwContents_handleJsAlert(
          env, obj.obj(), jurl.obj(), jmessage.obj(), js_result.obj());
      break;
    case content::JAVASCRIPT_MESSAGE_TYPE_CONFIRM:
      Java_AwContents_handleJsConfirm(
          env, obj.obj(), jurl.obj(), jmessage.obj(), js_result.obj());
      break;
    case content::JAVASCRIPT_MESSAGE_TYPE_PROMPT: {
      ScopedJavaLocalRef<jstring> jdefault_value(
          ConvertUTF16ToJavaString(env, default_prompt_text));
      Java_AwContents_handleJsPrompt(
          env, obj.obj(), jurl.obj(), jmessage.obj(),
          jdefault_value.obj(), js_result.obj());
      break;
    }
    default:
      NOTREACHED();
  }
}

void AwContents::RunBeforeUnloadDialog(
    const GURL& origin_url,
    const string16& message_text,
    const ScopedJavaLocalRef<jobject>& js_result) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jurl(ConvertUTF8ToJavaString(
      env, origin_url.spec()));
  ScopedJavaLocalRef<jstring> jmessage(ConvertUTF16ToJavaString(
      env, message_text));
  Java_AwContents_handleJsBeforeUnload(
          env, obj.obj(), jurl.obj(), jmessage.obj(), js_result.obj());
}

void AwContents::PerformLongClick() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwContents_performLongClick(env, obj.obj());
}

void AwContents::onReceivedHttpAuthRequest(
    const JavaRef<jobject>& handler,
    const std::string& host,
    const std::string& realm) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jhost = ConvertUTF8ToJavaString(env, host);
  ScopedJavaLocalRef<jstring> jrealm = ConvertUTF8ToJavaString(env, realm);
  Java_AwContents_onReceivedHttpAuthRequest(env, java_ref_.get(env).obj(),
                                            handler.obj(), jhost.obj(),
                                            jrealm.obj());
}

void AwContents::SetIoThreadClient(JNIEnv* env, jobject obj, jobject client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AwContentsIoThreadClientImpl::Associate(
      web_contents_.get(), ScopedJavaLocalRef<jobject>(env, client));
  int child_id = web_contents_->GetRenderProcessHost()->GetID();
  int route_id = web_contents_->GetRoutingID();
  AwResourceDispatcherHostDelegate::OnIoThreadClientReady(child_id, route_id);
}

void AwContents::SetInterceptNavigationDelegate(JNIEnv* env,
                                                jobject obj,
                                                jobject delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  InterceptNavigationDelegate::Associate(
      web_contents_.get(),
      make_scoped_ptr(new InterceptNavigationDelegate(env, delegate)));
}

static jint Init(JNIEnv* env,
                 jobject obj,
                 jobject web_contents_delegate,
                 jboolean private_browsing) {
  AwContents* tab = new AwContents(env, obj, web_contents_delegate,
                                   private_browsing);
  return reinterpret_cast<jint>(tab);
}

bool RegisterAwContents(JNIEnv* env) {
  return RegisterNativesImpl(env) >= 0;
}

jint AwContents::FindAllSync(JNIEnv* env, jobject obj, jstring search_string) {
  return GetFindHelper()->FindAllSync(
      ConvertJavaStringToUTF16(env, search_string));
}

void AwContents::FindAllAsync(JNIEnv* env, jobject obj, jstring search_string) {
  GetFindHelper()->FindAllAsync(ConvertJavaStringToUTF16(env, search_string));
}

void AwContents::FindNext(JNIEnv* env, jobject obj, jboolean forward) {
  GetFindHelper()->FindNext(forward);
}

void AwContents::ClearMatches(JNIEnv* env, jobject obj) {
  GetFindHelper()->ClearMatches();
}

void AwContents::ClearCache(
    JNIEnv* env,
    jobject obj,
    jboolean include_disk_files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  render_view_host_ext_->ClearCache();

  if (include_disk_files) {
    RemoveHttpDiskCache(web_contents_->GetBrowserContext(),
                        web_contents_->GetRoutingID());
  }
}

FindHelper* AwContents::GetFindHelper() {
  if (!find_helper_.get()) {
    find_helper_.reset(new FindHelper(web_contents_.get()));
    find_helper_->SetListener(this);
  }
  return find_helper_.get();
}

void AwContents::OnFindResultReceived(int active_ordinal,
                                      int match_count,
                                      bool finished) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwContents_onFindResultReceived(
      env, obj.obj(), active_ordinal, match_count, finished);
}

void AwContents::ScheduleComposite() {
  TRACE_EVENT0("AwContents", "AwContents::ScheduleComposite");

  if (is_composite_pending_)
    return;

  is_composite_pending_ = true;
  Invalidate();
}

void AwContents::Invalidate() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwContents_invalidate(env, obj.obj());
}

void AwContents::SetCompositorVisibility(bool visible) {
  if (compositor_visible_ != visible) {
    compositor_visible_ = visible;
    compositor_->SetVisible(compositor_visible_);
  }
}

void AwContents::OnSwapBuffersCompleted() {
}

base::android::ScopedJavaLocalRef<jbyteArray>
    AwContents::GetCertificate(JNIEnv* env,
                               jobject obj) {
  content::NavigationEntry* entry =
      web_contents_->GetController().GetActiveEntry();
  if (!entry)
    return ScopedJavaLocalRef<jbyteArray>();
  // Get the certificate
  int cert_id = entry->GetSSL().cert_id;
  scoped_refptr<net::X509Certificate> cert;
  bool ok = content::CertStore::GetInstance()->RetrieveCert(cert_id, &cert);
  if (!ok)
    return ScopedJavaLocalRef<jbyteArray>();

  // Convert the certificate and return it
  std::string der_string;
  net::X509Certificate::GetDEREncoded(cert->os_cert_handle(), &der_string);
  return base::android::ToJavaByteArray(env,
      reinterpret_cast<const uint8*>(der_string.data()), der_string.length());
}

void AwContents::RequestNewHitTestDataAt(JNIEnv* env, jobject obj,
                                         jint x, jint y) {
  render_view_host_ext_->RequestNewHitTestDataAt(x, y);
}

void AwContents::UpdateLastHitTestData(JNIEnv* env, jobject obj) {
  if (!render_view_host_ext_->HasNewHitTestData()) return;

  const AwHitTestData& data = render_view_host_ext_->GetLastHitTestData();
  render_view_host_ext_->MarkHitTestDataRead();

  // Make sure to null the Java object if data is empty/invalid.
  ScopedJavaLocalRef<jstring> extra_data_for_type;
  if (data.extra_data_for_type.length())
    extra_data_for_type = ConvertUTF8ToJavaString(
        env, data.extra_data_for_type);

  ScopedJavaLocalRef<jstring> href;
  if (data.href.length())
    href = ConvertUTF16ToJavaString(env, data.href);

  ScopedJavaLocalRef<jstring> anchor_text;
  if (data.anchor_text.length())
    anchor_text = ConvertUTF16ToJavaString(env, data.anchor_text);

  ScopedJavaLocalRef<jstring> img_src;
  if (data.img_src.is_valid())
    img_src = ConvertUTF8ToJavaString(env, data.img_src.spec());

  Java_AwContents_updateHitTestData(env,
                                    obj,
                                    data.type,
                                    extra_data_for_type.obj(),
                                    href.obj(),
                                    anchor_text.obj(),
                                    img_src.obj());
}

void AwContents::OnSizeChanged(JNIEnv* env, jobject obj,
                               int w, int h, int ow, int oh) {
  view_size_ = gfx::Size(w, h);
  if (view_clip_layer_.get())
    view_clip_layer_->setBounds(view_size_);
}

void AwContents::SetWindowViewVisibility(JNIEnv* env, jobject obj,
                                         bool window_visible,
                                         bool view_visible) {
  view_visible_ = window_visible && view_visible;
  Invalidate();
}

void AwContents::OnAttachedToWindow(JNIEnv* env, jobject obj, int w, int h) {
  view_size_ = gfx::Size(w, h);
  if (view_clip_layer_.get())
    view_clip_layer_->setBounds(view_size_);
}

void AwContents::OnDetachedFromWindow(JNIEnv* env, jobject obj) {
  view_visible_ = false;
  SetCompositorVisibility(false);
}

base::android::ScopedJavaLocalRef<jbyteArray>
AwContents::GetOpaqueState(JNIEnv* env, jobject obj) {
  // Required optimization in WebViewClassic to not save any state if
  // there has been no navigations.
  if (!web_contents_->GetController().GetEntryCount())
    return ScopedJavaLocalRef<jbyteArray>();

  Pickle pickle;
  if (!WriteToPickle(*web_contents_, &pickle)) {
    return ScopedJavaLocalRef<jbyteArray>();
  } else {
    return base::android::ToJavaByteArray(env,
       reinterpret_cast<const uint8*>(pickle.data()), pickle.size());
  }
}

jboolean AwContents::RestoreFromOpaqueState(
    JNIEnv* env, jobject obj, jbyteArray state) {
  // TODO(boliu): This copy can be optimized out if this is a performance
  // problem.
  std::vector<uint8> state_vector;
  base::android::JavaByteArrayToByteVector(env, state, &state_vector);

  Pickle pickle(reinterpret_cast<const char*>(state_vector.begin()),
                state_vector.size());
  PickleIterator iterator(pickle);

  return RestoreFromPickle(&iterator, web_contents_.get());
}

void AwContents::SetScrollForHWFrame(JNIEnv* env, jobject obj,
                                     int scroll_x, int scroll_y) {
  last_scroll_x_ = scroll_x;
  last_scroll_y_ = scroll_y;
}

void AwContents::SetPendingWebContentsForPopup(
    scoped_ptr<content::WebContents> pending) {
  if (pending_contents_.get()) {
    // TODO(benm): Support holding multiple pop up window requests.
    LOG(WARNING) << "Blocking popup window creation as an outstanding "
                 << "popup window is still pending.";
    MessageLoop::current()->DeleteSoon(FROM_HERE, pending.release());
    return;
  }
  pending_contents_ = pending.Pass();
}

void AwContents::FocusFirstNode(JNIEnv* env, jobject obj) {
  web_contents_->FocusThroughTabTraversal(false);
}

jint AwContents::ReleasePopupWebContents(JNIEnv* env, jobject obj) {
  return reinterpret_cast<jint>(pending_contents_.release());
}

void AwContents::OnPictureUpdated(int process_id, int render_view_id) {
  CHECK_EQ(web_contents_->GetRenderProcessHost()->GetID(), process_id);
  if (render_view_id != web_contents_->GetRoutingID())
    return;

  Invalidate();
}

}  // namespace android_webview
