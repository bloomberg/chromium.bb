// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab/thumbnail_tab_helper_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/thumbnails/simple_thumbnail_crop.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/thumbnails/thumbnailing_algorithm.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "jni/ThumbnailTabHelper_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

using thumbnails::ThumbnailingAlgorithm;
using thumbnails::ThumbnailingContext;
using thumbnails::ThumbnailService;

namespace {

const int kScrollbarWidthDp = 6;

void UpdateThumbnail(const ThumbnailingContext& context,
                     const SkBitmap& thumbnail) {
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(thumbnail);
  context.service->SetPageThumbnail(context, image);
}

void ProcessCapturedBitmap(
    const base::android::ScopedJavaGlobalRef<jobject>& jthumbnail_tab_helper,
    scoped_refptr<ThumbnailingContext> context,
    scoped_refptr<ThumbnailingAlgorithm> algorithm,
    const SkBitmap& bitmap,
    content::ReadbackResponse response) {
  if (response != content::READBACK_SUCCESS)
    return;

  // On success, we must be on the UI thread (on failure because of shutdown we
  // are not on the UI thread).
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_ThumbnailTabHelper_shouldSaveCapturedThumbnail(
          env, jthumbnail_tab_helper.obj())) {
    return;
  }

  algorithm->ProcessBitmap(context, base::Bind(&UpdateThumbnail), bitmap);
}

void CaptureThumbnailInternal(
    const base::android::ScopedJavaGlobalRef<jobject>& jthumbnail_tab_helper,
    content::WebContents* web_contents,
    scoped_refptr<ThumbnailingContext> context,
    scoped_refptr<ThumbnailingAlgorithm> algorithm,
    const gfx::Size& thumbnail_size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderWidgetHost* render_widget_host =
      web_contents->GetRenderViewHost()->GetWidget();
  content::RenderWidgetHostView* view = render_widget_host->GetView();
  if (!view)
    return;
  if (!view->IsSurfaceAvailableForCopy())
    return;

  gfx::Rect copy_rect = gfx::Rect(view->GetViewBounds().size());
  // Clip the pixels that will commonly hold a scrollbar, which looks bad in
  // thumbnails.
  copy_rect.Inset(0, 0, kScrollbarWidthDp, 0);
  if (copy_rect.IsEmpty())
    return;

  ui::ScaleFactor scale_factor =
      ui::GetSupportedScaleFactor(
          ui::GetScaleFactorForNativeView(view->GetNativeView()));
  context->clip_result = algorithm->GetCanvasCopyInfo(
      copy_rect.size(),
      scale_factor,
      &copy_rect,
      &context->requested_copy_size);

  // Workaround for a bug where CopyFromBackingStore() accepts different input
  // units on Android (DIP) vs on other platforms (pixels).
  // TODO(newt): remove this line once https://crbug.com/540497 is fixed.
  context->requested_copy_size = thumbnail_size;

  render_widget_host->CopyFromBackingStore(
      copy_rect, context->requested_copy_size,
      base::Bind(&ProcessCapturedBitmap, jthumbnail_tab_helper, context,
                 algorithm),
      kN32_SkColorType);
}

}  // namespace

// static
bool RegisterThumbnailTabHelperAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void InitThumbnailHelper(JNIEnv* env,
                                const JavaParamRef<jclass>& clazz,
                                const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  // Don't allow ThumbnailTabHelper to take thumbnail snapshots.
  // Currently the process is driven from Tab, but long term will
  // move into a Android specific tab helper.
  // Bug: crbug.com/157431
  ThumbnailTabHelper* thumbnail_tab_helper =
      ThumbnailTabHelper::FromWebContents(web_contents);
  if (thumbnail_tab_helper)
    thumbnail_tab_helper->set_enabled(false);
}

static void CaptureThumbnail(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             const JavaParamRef<jobject>& jthumbnail_tab_helper,
                             const JavaParamRef<jobject>& jweb_contents,
                             jint thumbnail_width_dp,
                             jint thumbnail_height_dp) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  scoped_refptr<ThumbnailService> thumbnail_service =
      ThumbnailServiceFactory::GetForProfile(profile);
  if (thumbnail_service.get() == nullptr ||
      !thumbnail_service->ShouldAcquirePageThumbnail(
          web_contents->GetLastCommittedURL())) {
    return;
  }

  const gfx::Size thumbnail_size(thumbnail_width_dp, thumbnail_height_dp);
  scoped_refptr<ThumbnailingAlgorithm> algorithm(
      new thumbnails::SimpleThumbnailCrop(thumbnail_size));

  scoped_refptr<ThumbnailingContext> context(new ThumbnailingContext(
      web_contents, thumbnail_service.get(), false /*load_interrupted*/));
  CaptureThumbnailInternal(
      base::android::ScopedJavaGlobalRef<jobject>(env, jthumbnail_tab_helper),
      web_contents, context, algorithm, thumbnail_size);
}
