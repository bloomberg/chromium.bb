// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_android.h"

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "jni/RenderFrameHostImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

// static
bool RenderFrameHostAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

RenderFrameHostAndroid::RenderFrameHostAndroid(
    RenderFrameHostImpl* render_frame_host)
    : render_frame_host_(render_frame_host) {}

RenderFrameHostAndroid::~RenderFrameHostAndroid() {
  ScopedJavaLocalRef<jobject> jobj = GetJavaObject();
  if (!jobj.is_null()) {
    Java_RenderFrameHostImpl_clearNativePtr(AttachCurrentThread(), jobj);
    obj_.reset();
  }
}

base::android::ScopedJavaLocalRef<jobject>
RenderFrameHostAndroid::GetJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (obj_.is_uninitialized()) {
    bool is_incognito = render_frame_host_->GetSiteInstance()
                            ->GetBrowserContext()
                            ->IsOffTheRecord();
    ScopedJavaLocalRef<jobject> local_ref = Java_RenderFrameHostImpl_create(
        env, reinterpret_cast<intptr_t>(this),
        render_frame_host_->delegate()->GetJavaRenderFrameHostDelegate(),
        is_incognito);
    obj_ = JavaObjectWeakGlobalRef(env, local_ref);
    return local_ref;
  }
  return obj_.get(env);
}

ScopedJavaLocalRef<jstring> RenderFrameHostAndroid::GetLastCommittedURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return ConvertUTF8ToJavaString(
      env, render_frame_host_->GetLastCommittedURL().spec());
}

}  // namespace content
