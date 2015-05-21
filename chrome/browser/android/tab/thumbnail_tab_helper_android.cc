// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab/thumbnail_tab_helper_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"
#include "components/history/core/browser/top_sites.h"
#include "content/public/browser/web_contents.h"
#include "jni/ThumbnailTabHelper_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

// static
bool RegisterThumbnailTabHelperAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void InitThumbnailHelper(JNIEnv* env,
                                jclass clazz,
                                jobject jweb_contents) {
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

static jboolean ShouldUpdateThumbnail(
    JNIEnv* env, jclass clazz, jobject jprofile, jstring jurl) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);

  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service =
      ThumbnailServiceFactory::GetForProfile(profile);
  return (thumbnail_service.get() != NULL &&
          thumbnail_service->ShouldAcquirePageThumbnail(url));
}

static void UpdateThumbnail(JNIEnv* env,
                            jclass clazz,
                            jobject jweb_contents,
                            jobject bitmap,
                            jboolean jat_top) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  Profile* profile = Profile::FromBrowserContext(
      web_contents->GetBrowserContext());

  gfx::JavaBitmap bitmap_lock(bitmap);
  SkBitmap sk_bitmap;
  gfx::Size size = bitmap_lock.size();
  SkColorType color_type = kN32_SkColorType;
  sk_bitmap.setInfo(
      SkImageInfo::Make(size.width(), size.height(),
                        color_type, kPremul_SkAlphaType), 0);
  sk_bitmap.setPixels(bitmap_lock.pixels());

  // TODO(nileshagrawal): Refactor this.
  // We were using some non-public methods from ThumbnailTabHelper. We need to
  // either add android specific logic to ThumbnailTabHelper or create our own
  // helper which is driven by the java app (will need to pull out some logic
  // from ThumbnailTabHelper to a common class).
  scoped_refptr<history::TopSites> ts = TopSitesFactory::GetForProfile(profile);
  if (!ts)
    return;

  // Compute the thumbnail score.
  ThumbnailScore score;
  score.at_top = jat_top;
  score.boring_score = color_utils::CalculateBoringScore(sk_bitmap);
  score.good_clipping = true;
  score.load_completed = !web_contents->IsLoading();

  gfx::Image image = gfx::Image::CreateFrom1xBitmap(sk_bitmap);
  const GURL& url = web_contents->GetURL();
  ts->SetPageThumbnail(url, image, score);
}
