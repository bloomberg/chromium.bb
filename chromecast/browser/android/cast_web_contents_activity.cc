// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/android/cast_web_contents_activity.h"

#include "content/public/browser/web_contents.h"
#include "jni/CastWebContentsActivity_jni.h"

namespace chromecast {
namespace shell {

namespace {
const void* kCastWebContentsActivityData;
const void* kCastWebContentsActivityKey =
    static_cast<const void*>(&kCastWebContentsActivityData);
}  // namespace

// static
void SetContentVideoViewEmbedder(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& webContents,
    const base::android::JavaParamRef<jobject>& embedder) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(webContents);
  DCHECK(web_contents);
  CastWebContentsActivity* activity =
      CastWebContentsActivity::Get(web_contents);
  activity->SetContentVideoViewEmbedder(embedder);
}

// static
bool CastWebContentsActivity::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
CastWebContentsActivity* CastWebContentsActivity::Get(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  CastWebContentsActivity* instance = static_cast<CastWebContentsActivity*>(
      web_contents->GetUserData(kCastWebContentsActivityKey));
  if (!instance) {
    instance = new CastWebContentsActivity(web_contents);
    web_contents->SetUserData(kCastWebContentsActivityKey, instance);
  }
  return instance;
}

CastWebContentsActivity::CastWebContentsActivity(
    content::WebContents* web_contents)
    : content_video_view_embedder_(
          base::android::ScopedJavaLocalRef<jobject>()) {}

CastWebContentsActivity::~CastWebContentsActivity() {}

base::android::ScopedJavaLocalRef<jobject>
CastWebContentsActivity::GetContentVideoViewEmbedder() {
  return base::android::ScopedJavaLocalRef<jobject>(
      content_video_view_embedder_);
}

void CastWebContentsActivity::SetContentVideoViewEmbedder(
    const base::android::JavaRef<jobject>& content_video_view_embedder) {
  content_video_view_embedder_ =
      base::android::ScopedJavaGlobalRef<jobject>(content_video_view_embedder);
}

}  // namespace shell
}  // namespace chromecast
