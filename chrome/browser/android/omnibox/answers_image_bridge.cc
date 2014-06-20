// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/answers_image_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/AnswersImage_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

using base::android::ScopedJavaLocalRef;
using base::android::ConvertUTF8ToJavaString;

namespace {

class AnswersImageObserverAndroid : public BitmapFetcherService::Observer {
 public:
  explicit AnswersImageObserverAndroid(JNIEnv* env,
                                       jobject java_answers_image_observer) {
    java_answers_image_observer_.Reset(env, java_answers_image_observer);
  }

  virtual ~AnswersImageObserverAndroid() {}

  // AnswersImageObserver:
  virtual void OnImageChanged(BitmapFetcherService::RequestId request_id,
                              const SkBitmap& answers_image) OVERRIDE {
    DCHECK(!answers_image.empty());

    JNIEnv* env = base::android::AttachCurrentThread();
    ScopedJavaLocalRef<jobject> java_bitmap =
        gfx::ConvertToJavaBitmap(&answers_image);
    Java_AnswersImageObserver_onAnswersImageChanged(
        env, java_answers_image_observer_.obj(), java_bitmap.obj());
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_answers_image_observer_;
};

}  // namespace

static void CancelAnswersImageRequest(JNIEnv* env,
                                      jclass,
                                      jobject java_profile,
                                      jint java_request_id) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(java_profile);
  DCHECK(profile);
  BitmapFetcherService* bitmap_fetcher_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile);
  bitmap_fetcher_service->CancelRequest(java_request_id);
}

static int RequestAnswersImage(JNIEnv* env,
                               jclass,
                               jobject java_profile,
                               jstring java_url,
                               jobject java_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(java_profile);
  DCHECK(profile);
  BitmapFetcherService* bitmap_fetcher_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile);
  std::string url;
  base::android::ConvertJavaStringToUTF8(env, java_url, &url);
  return bitmap_fetcher_service->RequestImage(
      GURL(url), new AnswersImageObserverAndroid(env, java_callback));
}

// static
bool RegisterAnswersImageBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
