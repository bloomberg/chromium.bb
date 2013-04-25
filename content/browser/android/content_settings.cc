// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_settings.h"

#include "base/android/jni_android.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "jni/ContentSettings_jni.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"

using base::android::GetFieldID;
using base::android::ScopedJavaLocalRef;

namespace content {

struct ContentSettings::FieldIds {
  FieldIds() { }

  FieldIds(JNIEnv* env) {
    // FIXME: we should be using a new GetFieldIDFromClassName() with caching.
    ScopedJavaLocalRef<jclass> clazz(base::android::GetClass(
        env, "org/chromium/content/browser/ContentSettings"));
    java_script_enabled =
        GetFieldID(env, clazz, "mJavaScriptEnabled", "Z");
  }

  // Field ids
  jfieldID java_script_enabled;
};

ContentSettings::ContentSettings(JNIEnv* env,
                         jobject obj,
                         WebContents* contents)
    : WebContentsObserver(contents),
      content_settings_(env, obj) {
}

ContentSettings::~ContentSettings() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = content_settings_.get(env);
  if (obj.obj()) {
    Java_ContentSettings_onNativeContentSettingsDestroyed(env, obj.obj(),
        reinterpret_cast<jint>(this));
  }
}

// static
bool ContentSettings::RegisterContentSettings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ContentSettings::SyncFromNativeImpl() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  if (!field_ids_)
    field_ids_.reset(new FieldIds(env));

  ScopedJavaLocalRef<jobject> scoped_obj = content_settings_.get(env);
  jobject obj = scoped_obj.obj();
  if (!obj)
    return;
  RenderViewHost* render_view_host = web_contents()->GetRenderViewHost();
  webkit_glue::WebPreferences prefs =
      render_view_host->GetDelegate()->GetWebkitPrefs();

  env->SetBooleanField(
      obj, field_ids_->java_script_enabled, prefs.javascript_enabled);
  base::android::CheckException(env);
}

void ContentSettings::SyncFromNative(JNIEnv* env, jobject obj) {
  SyncFromNativeImpl();
}

void ContentSettings::WebContentsDestroyed(WebContents* web_contents) {
  delete this;
}

static jint Init(JNIEnv* env, jobject obj, jint nativeContentViewCore) {
  WebContents* web_contents =
      reinterpret_cast<ContentViewCoreImpl*>(nativeContentViewCore)
          ->GetWebContents();
  ContentSettings* content_settings =
      new ContentSettings(env, obj, web_contents);
  return reinterpret_cast<jint>(content_settings);
}

}  // namespace content
