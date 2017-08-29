// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/android/cast_web_contents_activity.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"
#include "jni/CastWebContentsActivity_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace chromecast {
namespace shell {

namespace {
const void* kCastWebContentsActivityData;
const void* kCastWebContentsActivityKey =
    static_cast<const void*>(&kCastWebContentsActivityData);
}  // namespace

// static
void SetContentVideoViewEmbedder(JNIEnv* env,
                                 const JavaParamRef<jobject>& jcaller,
                                 const JavaParamRef<jobject>& webContents,
                                 const JavaParamRef<jobject>& embedder) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(webContents);
  DCHECK(web_contents);
  CastWebContentsActivity* activity =
      CastWebContentsActivity::Get(web_contents);
  activity->SetContentVideoViewEmbedder(embedder);
}

// static
CastWebContentsActivity* CastWebContentsActivity::Get(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  CastWebContentsActivity* instance = static_cast<CastWebContentsActivity*>(
      web_contents->GetUserData(kCastWebContentsActivityKey));
  if (!instance) {
    instance = new CastWebContentsActivity(web_contents);
    web_contents->SetUserData(kCastWebContentsActivityKey,
                              base::WrapUnique(instance));
  }
  return instance;
}

CastWebContentsActivity::CastWebContentsActivity(
    content::WebContents* web_contents)
    : content_video_view_embedder_(ScopedJavaLocalRef<jobject>()) {}

CastWebContentsActivity::~CastWebContentsActivity() {}

ScopedJavaLocalRef<jobject>
CastWebContentsActivity::GetContentVideoViewEmbedder() {
  return ScopedJavaLocalRef<jobject>(content_video_view_embedder_);
}

void CastWebContentsActivity::SetContentVideoViewEmbedder(
    const JavaParamRef<jobject>& content_video_view_embedder) {
  content_video_view_embedder_ =
      ScopedJavaGlobalRef<jobject>(content_video_view_embedder);
}

}  // namespace shell
}  // namespace chromecast
