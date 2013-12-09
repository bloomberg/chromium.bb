// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "jni/WebContentsImpl_jni.h"

using base::android::AttachCurrentThread;

namespace content {

// static
WebContents* WebContents::FromJavaWebContents(
    jobject jweb_contents_android) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!jweb_contents_android)
    return NULL;

  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(
          Java_WebContentsImpl_getNativePointer(AttachCurrentThread(),
                                                jweb_contents_android));
  if (!web_contents_android)
    return NULL;
  return web_contents_android->web_contents();
}

// static
bool WebContentsAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WebContentsAndroid::WebContentsAndroid(WebContents* web_contents)
    : web_contents_(web_contents),
      navigation_controller_(&(web_contents->GetController())) {
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(env,
             Java_WebContentsImpl_create(
                 env,
                 reinterpret_cast<intptr_t>(this),
                 navigation_controller_.GetJavaObject().obj()).obj());
}

WebContentsAndroid::~WebContentsAndroid() {
  Java_WebContentsImpl_destroy(AttachCurrentThread(), obj_.obj());
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

}  // namespace content
