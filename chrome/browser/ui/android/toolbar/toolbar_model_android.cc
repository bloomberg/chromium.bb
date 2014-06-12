// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/toolbar_model_android.h"

#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "content/public/browser/web_contents.h"
#include "jni/ToolbarModel_jni.h"

using base::android::ScopedJavaLocalRef;

ToolbarModelAndroid::ToolbarModelAndroid(JNIEnv* env, jobject jdelegate)
    : toolbar_model_(new ToolbarModelImpl(this)),
      weak_java_delegate_(env, jdelegate) {
}

ToolbarModelAndroid::~ToolbarModelAndroid() {
}

void ToolbarModelAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

ScopedJavaLocalRef<jstring> ToolbarModelAndroid::GetText(JNIEnv* env,
                                                         jobject obj) {
  return base::android::ConvertUTF16ToJavaString(env,
                                                 toolbar_model_->GetText());
}

ScopedJavaLocalRef<jstring> ToolbarModelAndroid::GetQueryExtractionParam(
    JNIEnv* env,
    jobject obj) {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return ScopedJavaLocalRef<jstring>();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  UIThreadSearchTermsData search_terms_data(profile);
  return base::android::ConvertUTF8ToJavaString(
      env, chrome::InstantExtendedEnabledParam(true));
}

ScopedJavaLocalRef<jstring> ToolbarModelAndroid::GetCorpusChipText(
    JNIEnv* env,
    jobject obj) {
  return base::android::ConvertUTF16ToJavaString(
      env,
      toolbar_model_->GetCorpusNameForMobile());
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

bool ToolbarModelAndroid::InTabbedBrowser() const {
  return true;
}

// static
bool ToolbarModelAndroid::RegisterToolbarModelAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
jlong Init(JNIEnv* env, jobject obj, jobject delegate) {
  ToolbarModelAndroid* toolbar_model = new ToolbarModelAndroid(env, delegate);
  return reinterpret_cast<intptr_t>(toolbar_model);
}
