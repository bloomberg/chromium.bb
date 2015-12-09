// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/android/toolbar.h"

#include "base/android/jni_string.h"
#include "base/lazy_instance.h"
#include "blimp/client/session/blimp_client_session_android.h"
#include "blimp/net/null_blimp_message_processor.h"
#include "jni/Toolbar_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

namespace blimp {

namespace {

const int kDummyTabId = 0;

base::LazyInstance<blimp::NullBlimpMessageProcessor>
    g_null_message_processor = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& jobj,
                  const JavaParamRef<jobject>& blimp_client_session) {
  BlimpClientSession* client_session =
      BlimpClientSessionAndroid::FromJavaObject(env,
                                                blimp_client_session.obj());

  // TODO(dtrainor): Pull the feature object from the BlimpClientSession and
  // use it instead of the NavigationMessageProcessor.
  ALLOW_UNUSED_LOCAL(client_session);

  return reinterpret_cast<intptr_t>(new Toolbar(env, jobj));
}

// static
bool Toolbar::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

Toolbar::Toolbar(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jobj)
    : navigation_message_processor_(g_null_message_processor.Pointer()) {
  java_obj_.Reset(env, jobj);

  navigation_message_processor_.SetDelegate(kDummyTabId, this);
}

Toolbar::~Toolbar() {
  navigation_message_processor_.RemoveDelegate(kDummyTabId);
}

void Toolbar::Destroy(JNIEnv* env, jobject jobj) {
  delete this;
}

void Toolbar::OnUrlTextEntered(JNIEnv* env, jobject jobj, jstring text) {
  navigation_message_processor_.NavigateToUrlText(
      kDummyTabId,
      base::android::ConvertJavaStringToUTF8(env, text));
}

void Toolbar::OnReloadPressed(JNIEnv* env, jobject jobj) {
  navigation_message_processor_.Reload(kDummyTabId);
}

void Toolbar::OnForwardPressed(JNIEnv* env, jobject jobj) {
  navigation_message_processor_.GoForward(kDummyTabId);
}

jboolean Toolbar::OnBackPressed(JNIEnv* env, jobject jobj) {
  navigation_message_processor_.GoBack(kDummyTabId);

  // TODO(dtrainor): Find a way to determine whether or not we're at the end of
  // our history stack.
  return true;
}

void Toolbar::OnUrlChanged(int tab_id, const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl(
      base::android::ConvertUTF8ToJavaString(env, url.spec()));

  Java_Toolbar_onEngineSentUrl(env, java_obj_.obj(), jurl.obj());
}

void Toolbar::OnFaviconChanged(int tab_id, const SkBitmap& favicon) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jfavicon(gfx::ConvertToJavaBitmap(&favicon));

  Java_Toolbar_onEngineSentFavicon(env, java_obj_.obj(), jfavicon.obj());
}

void Toolbar::OnTitleChanged(int tab_id, const std::string& title) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jtitle(
      base::android::ConvertUTF8ToJavaString(env, title));

  Java_Toolbar_onEngineSentTitle(env, java_obj_.obj(), jtitle.obj());
}

void Toolbar::OnLoadingChanged(int tab_id, bool loading) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_Toolbar_onEngineSentLoading(env,
                                   java_obj_.obj(),
                                   static_cast<jboolean>(loading));
}

}  // namespace blimp
