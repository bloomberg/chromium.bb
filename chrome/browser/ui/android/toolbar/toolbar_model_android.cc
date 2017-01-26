// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/toolbar_model_android.h"

#include "base/android/jni_string.h"
#include "components/toolbar/toolbar_model_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "jni/ToolbarModel_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

ToolbarModelAndroid::ToolbarModelAndroid(JNIEnv* env, jobject jdelegate)
    : toolbar_model_(new ToolbarModelImpl(this, content::kMaxURLDisplayChars)),
      weak_java_delegate_(env, jdelegate) {}

ToolbarModelAndroid::~ToolbarModelAndroid() {
}

void ToolbarModelAndroid::Destroy(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jstring> ToolbarModelAndroid::GetText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return base::android::ConvertUTF16ToJavaString(
      env, toolbar_model_->GetFormattedURL(nullptr));
}

content::WebContents* ToolbarModelAndroid::GetActiveWebContents() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = weak_java_delegate_.get(env);
  if (!jdelegate.obj())
    return NULL;
  ScopedJavaLocalRef<jobject> jweb_contents =
      Java_ToolbarModelDelegate_getActiveWebContents(env, jdelegate);
  return content::WebContents::FromJavaWebContents(jweb_contents);
}

// static
bool ToolbarModelAndroid::RegisterToolbarModelAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& delegate) {
  ToolbarModelAndroid* toolbar_model = new ToolbarModelAndroid(env, delegate);
  return reinterpret_cast<intptr_t>(toolbar_model);
}
