// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/toolbar_model_android.h"

#include "base/android/jni_string.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/ssl_status.h"
#include "jni/ToolbarModel_jni.h"
#include "net/cert/x509_certificate.h"

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
  return base::android::ConvertUTF16ToJavaString(env,
                                                 toolbar_model_->GetText());
}

ScopedJavaLocalRef<jstring> ToolbarModelAndroid::GetCorpusChipText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return base::android::ConvertUTF16ToJavaString(
      env,
      toolbar_model_->GetCorpusNameForMobile());
}

jboolean ToolbarModelAndroid::WouldReplaceURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return toolbar_model_->WouldReplaceURL();
}

content::WebContents* ToolbarModelAndroid::GetActiveWebContents() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = weak_java_delegate_.get(env);
  if (!jdelegate.obj())
    return NULL;
  ScopedJavaLocalRef<jobject> jweb_contents =
      Java_ToolbarModelDelegate_getActiveWebContents(env, jdelegate.obj());
  return content::WebContents::FromJavaWebContents(jweb_contents.obj());
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
