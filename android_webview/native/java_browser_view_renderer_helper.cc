// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/java_browser_view_renderer_helper.h"

#include <android/bitmap.h>

#include "android_webview/common/aw_switches.h"
#include "android_webview/public/browser/draw_sw.h"
#include "base/debug/trace_event.h"
#include "jni/JavaBrowserViewRendererHelper_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/utils/SkCanvasStateUtils.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

namespace {

// Provides software rendering functions from the Android glue layer.
// Allows preventing extra copies of data when rendering.
AwDrawSWFunctionTable* g_sw_draw_functions = NULL;

class ScopedPixelAccess {
 public:
  ScopedPixelAccess(JNIEnv* env, jobject java_canvas) : pixels_(NULL) {
    if (g_sw_draw_functions && !switches::ForceAuxiliaryBitmap())
      pixels_ = g_sw_draw_functions->access_pixels(env, java_canvas);
  }

  ~ScopedPixelAccess() {
    if (pixels_)
      g_sw_draw_functions->release_pixels(pixels_);
  }

  AwPixelInfo* pixels() { return pixels_; }

 private:
  AwPixelInfo* pixels_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedPixelAccess);
};

}  // namespace

// static
void JavaBrowserViewRendererHelper::SetAwDrawSWFunctionTable(
    AwDrawSWFunctionTable* table) {
  g_sw_draw_functions = table;
}

// static
JavaBrowserViewRendererHelper* JavaBrowserViewRendererHelper::GetInstance() {
  static JavaBrowserViewRendererHelper* g_instance =
      new JavaBrowserViewRendererHelper;
  return g_instance;
}

// static
BrowserViewRendererJavaHelper* BrowserViewRendererJavaHelper::GetInstance() {
  return JavaBrowserViewRendererHelper::GetInstance();
}

JavaBrowserViewRendererHelper::JavaBrowserViewRendererHelper() {}

JavaBrowserViewRendererHelper::~JavaBrowserViewRendererHelper() {}

bool JavaBrowserViewRendererHelper::RenderViaAuxilaryBitmapIfNeeded(
    jobject java_canvas,
    const gfx::Vector2d& scroll_correction,
    const gfx::Rect& auxiliary_bitmap_rect,
    RenderMethod render_source) {
  TRACE_EVENT0("android_webview", "RenderViaAuxilaryBitmapIfNeeded");

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedPixelAccess auto_release_pixels(env, java_canvas);
  AwPixelInfo* pixels = auto_release_pixels.pixels();
  if (pixels && pixels->state) {
    skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(
        SkCanvasStateUtils::CreateFromCanvasState(pixels->state));

    // Workarounds for http://crbug.com/271096: SW draw only supports
    // translate & scale transforms, and a simple rectangular clip.
    if (canvas && (!canvas->isClipRect() ||
                   (canvas->getTotalMatrix().getType() &
                    ~(SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask)))) {
      canvas.clear();
    }
    if (canvas) {
      canvas->translate(scroll_correction.x(), scroll_correction.y());
      return render_source.Run(canvas.get());
    }
  }
  return RenderViaAuxilaryBitmap(env,
                                 java_canvas,
                                 scroll_correction,
                                 auxiliary_bitmap_rect,
                                 render_source);
}

bool JavaBrowserViewRendererHelper::RenderViaAuxilaryBitmap(
    JNIEnv* env,
    jobject java_canvas,
    const gfx::Vector2d& scroll_correction,
    const gfx::Rect& auxiliary_bitmap_rect,
    const RenderMethod& render_source) {
  // Render into an auxiliary bitmap if pixel info is not available.
  ScopedJavaLocalRef<jobject> jcanvas(env, java_canvas);
  TRACE_EVENT0("android_webview", "RenderToAuxBitmap");

  if (auxiliary_bitmap_rect.width() <= 0 || auxiliary_bitmap_rect.height() <= 0)
    return false;

  ScopedJavaLocalRef<jobject> jbitmap(
      Java_JavaBrowserViewRendererHelper_createBitmap(
          env,
          auxiliary_bitmap_rect.width(),
          auxiliary_bitmap_rect.height(),
          jcanvas.obj()));
  if (!jbitmap.obj())
    return false;

  if (!RasterizeIntoBitmap(env,
                           jbitmap,
                           auxiliary_bitmap_rect.x() - scroll_correction.x(),
                           auxiliary_bitmap_rect.y() - scroll_correction.y(),
                           render_source)) {
    return false;
  }

  Java_JavaBrowserViewRendererHelper_drawBitmapIntoCanvas(
      env,
      jbitmap.obj(),
      jcanvas.obj(),
      auxiliary_bitmap_rect.x(),
      auxiliary_bitmap_rect.y());
  return true;
}

bool RegisterJavaBrowserViewRendererHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

bool JavaBrowserViewRendererHelper::RasterizeIntoBitmap(
    JNIEnv* env,
    const JavaRef<jobject>& jbitmap,
    int scroll_x,
    int scroll_y,
    const JavaBrowserViewRendererHelper::RenderMethod& renderer) {
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
    SkImageInfo info =
        SkImageInfo::MakeN32Premul(bitmap_info.width, bitmap_info.height);
    SkBitmap bitmap;
    bitmap.installPixels(info, pixels, bitmap_info.stride);

    SkCanvas canvas(bitmap);
    canvas.translate(-scroll_x, -scroll_y);
    succeeded = renderer.Run(&canvas);
  }

  if (AndroidBitmap_unlockPixels(env, jbitmap.obj()) < 0) {
    LOG(ERROR) << "Error unlocking java bitmap pixels.";
    return false;
  }

  return succeeded;
}

}  // namespace android_webview
